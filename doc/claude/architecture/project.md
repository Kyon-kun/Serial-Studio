# Architecture — Project Model, Files & Importers

> Part of the architecture corpus ([index](../architecture.md)). Read this file in full
> before touching ProjectModel/ProjectEditor, project JSON serialization, backups, or the
> importers. The ProjectModel ctor closure is a protected surface — see
> [startup.md](startup.md) and CLAUDE.md "Startup & Composition Root".

## ProjectModel / ProjectEditor Split

- `ProjectModel` (`Cpp_JSON_ProjectModel`): pure data — groups, actions, config, file I/O.
- `ProjectEditor` (`Cpp_JSON_ProjectEditor`): editor controller — tree model, form models,
  selection, comboboxes.
- QML enum access: `ProjectModel.SomeEnum` / `ProjectEditor.SomeEnum` — **not** `Cpp_JSON_*`.
- `groupsChanged` → `buildTreeModel()` is `Qt::QueuedConnection`; selection runs via hint
  signals afterwards.
- Title edits update the tree item in-place via `m_*Items` — never call a mutating
  `ProjectModel` function on every keystroke.

## On-Disk Change Detection — `ProjectModel` File Watcher

- A `QFileSystemWatcher` on `m_filePath` detects external edits: 500 ms debounce →
  SHA-256 content compare against `m_diskFileHash` → prompt to reload (or notification +
  `setModified(true)` in suppressed/API mode). Deletion posts a warning and dirties the
  project so a save can recreate the file. Signal: `projectFileChangedOnDisk()`.
- **Invariant**: every successful disk write or load must re-arm the watcher + hash via
  `watchProjectFile()`. `writeProjectFile()`, `loadFromJsonDocument()`, and `newJsonFile()`
  already do; a new save/load path that bypasses them will make self-writes look like
  external edits (QSaveFile's atomic rename also drops the watch on some platforms).

## Rolling Backups — `BackupManager`

- Auto-snapshots the project on a 5s debounce. The **whole-project SHA-1** over
  `serializeToJson()` is the sole write arbiter: identical content never duplicates a snapshot,
  any byte difference (incl. `frameParserCode`) does. Restore round-trips parser code + engines.
- Trigger is **decoupled from the dirty flag**. `setModified()` suppresses the flag for a
  structurally empty project (no groups/actions/tables/workspaces), but still emits
  `contentTouched` so parser-only edits on an empty project reach the snapshot path. Wire any new
  "edit that should back up but not dirty the project" through `contentTouched`, not a forced
  `modifiedChanged`.

## Multi-Source Architecture

- `DataModel::Source` entries in `Frame.h`. `FrameBuilder::hotpathRxSourceFrame(sourceId, data)`
  routes per-source frames. `FrameParser` keeps one engine per `sourceId`.
- GPL: `openJsonFile()` truncates `m_sources` to 1; `addSource()` is gated by
  `BUILD_COMMERCIAL`.
- Bus type change: set `m_awaitingContextRebuild`, wait for one-shot `contextsRebuilt`, then
  `buildSourceModel`. Don't force-rebuild on selection.

## Project File JSON Keys — `Keys::` Namespace

Every JSON key used in `.json`/`.ssproj` files is declared in `namespace Keys` at the top
of `app/src/DataModel/Frame.h` as `inline constexpr QLatin1StringView` (alias `KeyView`).

- **Never hardcode** `"busType"`, `"frameStart"`, etc. in writers/readers or MCP handlers —
  use `Keys::BusType`, `Keys::FrameStart`. (`code-verify.py:keys-hardcoded-literal`.)
- `ss_jsr(obj, Keys::Foo, default)` is the canonical reader.
- **Legacy aliases (read canonical first, write both)**: `checksum` ↔ `checksumAlgorithm`,
  `decoder` ↔ `decoderMethod`. Older Serial Studio versions still load 3.3+ files.
- **Schema versioning** (`kSchemaVersion = 1`): `ProjectModel::serializeToJson()` always
  stamps `schemaVersion`, `writerVersion`, `writerVersionAtCreation`. Live runtime frames
  broadcast over the API keep `schemaVersion = 0` — `Frame::serialize` only emits when the
  Frame already carries a stamp. `current_writer_version()` lives in `Frame.cpp` so
  `Frame.h` doesn't need `AppInfo.h`.
- Use `obj.contains(Keys::Foo)` to detect "field absent", not `std::isnan` on a default-zero
  read.

## Modbus Map Importer (Pro)

`DataModel::ModbusMapImporter` imports CSV/XML/JSON →
auto-generates a Modbus project; preview in `ModbusPreviewDialog.qml`. Pairs with
`IO::Drivers::Modbus::generateProject`.

## Importer Parser Output

The Modbus map and DBC importers generate **commented,
declarative Lua parsers** (`frameParserLanguage = Lua`), not native map templates — the
`modbus_register_map` / `can_signal_map` templates and `MapTemplates.cpp` were removed
(projects that referenced them must be re-imported). The generated parser decodes through
a spec table (one line per signal/register, DBC `CM_` comments inlined) and publishes
**physical values into per-group data tables** via `tableSet`; every dataset is
`virtual: true` with a Lua `tableGet` transform (`ImporterCommon.h::applyTableTransform`),
so nothing depends on positional parser channels (parsers return `{ 0 }` as a dummy row —
an empty return would skip the frame and starve the transforms). The Modbus Lua keeps the
driver's round-robin poll cursor as chunk-local state and resyncs on the response function
code (RegBool decodes the whole word; bit path only for coil/discrete blocks); the CAN Lua
mirrors the DBC bit semantics (Motorola sawtooth walk, Intel LSB-first, Qt endian flag
verbatim) — both pinned by `test_cpp_regressions.py` R14/R15 against the codegen. The CAN
driver publishes standard frames as `[ID_hi, ID_lo, DLC, data...]` (11-bit id, byte 0 top
bit always clear) and extended frames as `[0x80|ID28..24, ID23..16, ID15..8, ID7..0, DLC,
data...]` — bit 7 of byte 0 selects the form, `write()` mirrors it, and the generated Lua's
`frame_id()` decodes both (pinned by R17). The Modbus *driver* quick-connect
(`buildFrameParser`) and the Protobuf importer still generate their own script parsers.

## Importer Dashboards (DBC + Modbus Map)

Summary-first projects. Every group is a
DataGrid (DBC still detects GPS / accelerometer / gyroscope groups), analog datasets carry
plot + bar/gauge/meter toggles disclosed on demand via the data grid's pop-out buttons,
boolean signals are LEDs with an explicit `[0.5, 1]` Ok alarm band (no reliance on the
runtime `ledHigh` synthesis), DBC `VAL_` value tables become Lua transforms returning the
label text (only when factor = 1 / offset = 0), and `displayFormat` decimals derive from
the scaling factor. Generated bar/gauge/meter datasets get the analog display policy
(`ImporterCommon.h::applyAnalogDisplayPolicy`): integer-aligned tick counts (0-10 → 11
ticks, 0-150 → 7) and integer labels once the range spans more than one unit. Both
importers seed **customized workspaces** — a leading Overview
aggregating every group's refs (multi-group projects only), then one workspace per group,
each holding only the group-widget ref (+ LED panel ref), user-range IDs (≥ 5000 so the
load-time auto-range remap never fires) — through
`Importers/ImporterCommon.h::finalizeImportedProject`, which also assigns group uniqueIds,
serializes the data tables, and stamps `schemaVersion` + `nextUniqueId`: omit those stamps
and the loader treats the import as an older-schema project and silently drops the seeded
workspaces.
