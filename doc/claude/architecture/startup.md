# Architecture — Startup, Composition Root & AppState

> Part of the architecture corpus ([index](../architecture.md)). Read this file in full
> before touching ModuleManager, AppState, operation modes, singleton construction, or the
> updater. The ctor-closure rules are also summarized in CLAUDE.md under
> "Startup & Composition Root — Non-Negotiable".

## Composition Root & Construction Order (ModuleManager)

`Misc::ModuleManager` is the composition root in all but name. `initializeQmlInterface` starts the
timers, wires everything through `setupCrossModuleConnections()` (an ordered run of
`setupExternalConnections()` calls followed by `restoreLastProject()`), installs the message
handler, registers the `Cpp_*` QML context properties, then loads `main.qml`. Three standing
invariants hold it together:

- **All `setupExternalConnections()` run before `restoreLastProject()`.** `restoreLastProject` is
  the last call inside `setupCrossModuleConnections` (ModuleManager.cpp:699); a module that reacts
  to project load must have its wiring in place first.
- **Context properties come after wiring, before the QML load.** `registerCoreContextProperties` /
  `registerCommercialContextProperties` / `registerAppMetadataProperties` run after
  `setupCrossModuleConnections()` and before `registerImageProvidersAndLoadQml` (`m_engine.load`),
  so QML never binds a half-wired object.
- **`qInstallMessageHandler(MessageHandler)` runs only after `Console::Handler` and
  `NotificationCenter` exist.** `MessageHandler` (ModuleManager.cpp:141) constructs both on the
  first warning **from any thread**, and `Console::Handler`'s ctor pulls `CommonFonts` (which
  touches the font database, GUI-thread-only). Installing the handler after
  `setupCrossModuleConnections` forces both onto the GUI thread first, so no worker-thread warning
  triggers their first construction off-thread.

**Pinned instantiation order** (the topological order the modules must construct in): `Translator`,
`TimerEvents`, `CommonFonts`, `WorkspaceManager`, `NotificationCenter`, `ThemeManager`,
`ExtensionManager`, `ControlScript`, **`ProjectModel` before `AppState`**, [`LemonSqueezy` /
`MachineID`, commercial], `FrameBuilder`, `IO::ConnectionManager`, `Console::Handler`,
`API::Server`, `CSV::Player`, `MDF4::Player`, [`Sessions::Player`], the exports, `FrameParser`, and
`UI::Dashboard` **last** (its ctor wires multiple core modules, the file/session players, and
`TimerEvents`).

**The `ProjectModel`-before-`AppState` rule kills a live hazard.** `AppState`'s ctor calls
`deriveFrameConfig()`, whose ProjectFile branch calls `ProjectModel::instance()` (AppState.cpp), so
on a machine whose saved `operation_mode` is ProjectFile, ProjectModel is constructed *inside*
AppState's ctor; on a QuickPlot machine it is constructed later. `ProjectModel`'s ctor then calls
`newJsonFile()`, which emits `groupsChanged` while AppState is still mid-init (the fenced comment at
ProjectModel.cpp:162 exists for exactly this reason). Constructing ProjectModel first makes the
settings-conditional edge impossible.

`ModuleManager::instantiateCoreModules()` (ModuleManager.cpp:611, called first inside
`setupCrossModuleConnections`) now enforces this order directly in code: it force-constructs
every core singleton in the pinned sequence above (ProjectModel before AppState; the commercial
`MachineID` / `LemonSqueezy` pair under `BUILD_COMMERCIAL`; Dashboard last), replacing the old
settings-dependent lazy first-use order. Spec `doc/claude/specs/0001-composition-root/` keeps the
ctor-edge proof.

**The pinned order creates a protected surface: everything reachable from ProjectModel's ctor
(`newJsonFile()`, `watchProjectFile()`, `scheduleAutoSave()`, `ControlScript::setCode`) runs
BEFORE AppState and Dashboard exist.** Calling `AppState::instance()` or `UI::Dashboard::instance()`
from that closure recurses the Meyers guard on ProjectFile machines and aborts at startup
(`__cxa_guard_acquire detected recursive initialization` — this shipped and crashed once, 2026-07-07).
`newJsonFile()`'s Dashboard sync is gated on `m_initialized` (set at the end of the ctor);
`scheduleAutoSave()` is safe only because the empty-`m_filePath` early-return precedes its AppState
read. Any new code in this closure must keep those guards or add its own `m_initialized` gate.

**MMCSS coexistence contract (Windows).** Registering the main thread with MMCSS
(`AvSetMmThreadCharacteristics`) **before the Qt message handler is installed** — or treating
the `QThread::start` priority warning it triggers as a real failure — is a mistake. Qt's default
`QThread::InheritPriority` reads the creator's raw priority (MMCSS-managed ~25, not a
`THREAD_PRIORITY_*` constant) and feeds it back to `SetThreadPriority`, which rejects it — the
thread still starts and lands at NORMAL, **its exact pre-MMCSS inherited value**, so the failure
is benign; explicit priorities (named constants, e.g. `Audio.cpp` `setPriority(HighestPriority)`)
are unaffected. The contract: register only via
`Platform::AppPlatform::registerIngestThreadWithMmcss()`, called AFTER `qInstallMessageHandler`
(ModuleManager) so the targeted filter eats the warning, and never start a QThread expecting it
to inherit the boosted band.

## AppState — Single Source of Truth

`AppState` (`Cpp_AppState`) owns `operationMode`, `projectFilePath`, `frameConfig`.

- `operationMode` persists to QSettings; everything else reacts to `operationModeChanged()`.
- `frameConfig` is derived from mode + project source[0]; emits `frameConfigChanged(config)`.
- Init order: all `setupExternalConnections()` first, then `restoreLastProject()`.
- `setOperationMode()` guard-returns if unchanged.

## Operation Modes

| Mode | Delimiters | CSV delim | JS parser | Dashboard |
|------|-----------|-----------|-----------|-----------|
| ProjectFile (0) | Per-source | Via JS | Yes | Yes |
| ConsoleOnly (1) | None (short-circuits) | N/A | No | No |
| QuickPlot (2) | Line-based (CR/LF/CRLF) | Comma | No | Yes |

ConsoleOnly (replaced DeviceSendsJSON, 2026-04) bypasses CircularBuffer + queue;
`FrameBuilder::hotpathRxFrame` is a no-op; raw bytes reach the terminal via
`DeviceManager::rawDataReceived`.

## Packaging-Aware Updater

`ModuleManager::configureUpdater()` resolves the QSimpleUpdater
appcast key (repo-root `updates.json`) in three tiers: the CI-stamped `ss-config.json`
(`packageType` + `arch`) read from `applicationDirPath()` (macOS also `../Resources`), then
runtime probing (`APPIMAGE` env var on Linux; `GetCurrentPackageFullName` on Windows so a
Store install is never offered the MSI), then the legacy per-OS keys. `windows-msix` is
open-url-only (Store owns updates). Three things must stay in sync: the ci.yml stamp steps
(one per package; deb/rpm are two separate ldnp runs, macOS stamps before codesign, MSI via
`-DSS_PACKAGE_TYPE` in `app/CMakeLists.txt`), the key table in `ModuleManager.cpp`, and the
`updates.json` keys (shape pinned by `tests/unit/test_updates_manifest.py`). Dev builds have
no stamp and keep today's behavior.
