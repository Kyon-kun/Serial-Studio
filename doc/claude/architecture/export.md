# Architecture — Export & Sessions DB

> Part of the architecture corpus ([index](../architecture.md)). Read this file in full
> before touching CSV/MDF4 export, the Sessions database, or replay.

## Export Architecture & Sessions DB (Pro)

- `DataModel::ExportSchema` (`ExportSchema.h`): shared column layout. `buildExportSchema(frame)`
  produces sorted columns + `uniqueIdToColumnIndex` map. CSV and MDF4 export raw + transformed.
- **Session DB lives in `app/src/Sessions/`** (NOT `app/src/SQLite/`):
  - `Sessions::DatabaseManager` — singleton owning the open `.db`; backs `app/qml/DatabaseExplorer/`.
  - `Sessions::Export` (`Sessions/Export.h/.cpp`): `FrameConsumer`-based; tables
    `sessions/columns/readings/raw_bytes/table_snapshots`; second lock-free queue for raw
    bytes via `ConnectionManager::onRawDataReceived`. WAL mode, batch transactions.
  - `Sessions::Player`: replays a stored session through the FrameBuilder pipeline using the
    **final** (post-transform) reading columns, with a uid->cell replay column map installed via
    `FrameBuilder::setReplayColumnMap` (same mechanism as MDF4). **All three players count as
    final-value players** (`SerialStudio::isFinalValuePlayerOpen`), so per-dataset transforms
    never re-run during playback — they read live inputs (data tables) that don't exist then.
    Raw columns are only a fallback for pre-final-column session files.
  - **Replay payload rows are RFC-4180 quoted**: players synthesize rows with
    `DataModel::joinReplayRow` and FrameBuilder splits them with `splitReplayChannels` /
    `splitReplayRow` (`FrameParserPipeline.h`), so string values containing commas/quotes
    survive replay. The live QuickPlot split (`splitQuickPlotChannels`) is untouched — the
    quote-aware splitter only runs when `m_playerOpen` is set.
  - **`table_snapshots` capture**: `Sessions::Export::captureTableSnapshots` (main thread,
    `TimerEvents::timeout1Hz`) diffs `FrameBuilder::tableStore().snapshot()` (skipping the
    `__datasets__` system table) against the last tick and enqueues changed registers to the
    worker, which batches them into `table_snapshots`. Replay does NOT need them (finals are
    replayed); they exist for post-hoc inspection.
  - Per-sample tables use **surrogate rowid PKs** (`reading_id`, `raw_id`, `snapshot_id`
    `INTEGER PRIMARY KEY AUTOINCREMENT`) with covering indexes on
    `(session_id, unique_id, timestamp_ns)` and `(session_id, timestamp_ns)`. Use plain
    `INSERT` — **never `INSERT OR IGNORE`** — `timestamp_ns` collisions are routine.
  - Break ts ties with `reading_id` in ORDER BY / MIN/MAX subqueries. `DISTINCT timestamp_ns`
    stats undercount on collisions.
