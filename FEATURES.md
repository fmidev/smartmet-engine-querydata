# smartmet-engine-querydata — Feature List

A structured inventory of capabilities provided by the querydata
engine. Use as a checklist when drafting release notes. When new
functionality is added, append the new entry under the matching
section (and bump the *Last updated* line at the bottom).

`smartmet-engine-querydata` (output: `querydata.so`) is the SmartMet
Server engine that gives plugins shared access to gridded weather
forecast data in FMI's **QueryData** format. It memory-maps `.sqd`
files from disk or NFS, performs spatial / temporal / vertical
interpolation, applies DEM-based corrections, and automatically picks
the best data source for any requested region.

---

## 1. Engine surface for plugins

- **`Engine` base class** — inherits `Spine::SmartMetEngine`; all
  virtuals throw "engine is disabled" by default. Plugins always see
  the base class.
- **`EngineImpl` final** — the real implementation. Created via
  `EngineImpl::create(configfile)`. Plugins never reference it
  directly.
- **`Engine::get(producer)`** — returns a `Q` handle for a producer.
- **`Engine::getEngineContents*`** — list producers / origin times
  with formatted time and projection columns.
- **`Engine::getEngineMetadataWithOptions(options)`** — structured
  metadata for filter / search.
- **`Engine::getWorldCoordinates*`** — projected coordinate matrices
  for arbitrary spatial references.
- **`Engine::getValuesDefault` / `getValuesForParam`** — bulk grid
  value access with hash-based caching.

## 2. `Q` data-access handle

`Q` (`std::shared_ptr<QImpl>`) is the primary handle plugins receive.
It wraps `NFmiFastQueryInfo` from newbase:

- **Parameter iteration** — `param()`, `nextParam()`, `resetParam()`.
- **Location iteration** — `location()`, `nextLocation()`, plus
  point/grid access.
- **Time iteration** — `time()`, `nextTime()`, `firstTime()`,
  `lastTime()`.
- **Level iteration** — `level()`, `nextLevel()`, level-type queries.
- **Single-value reads** — `value(...)`, `interpolatedValue(...)`.
- **Spatial interpolation** — bilinear / biquadratic / nearest at
  arbitrary lat/lon.
- **Temporal interpolation** — linear interpolation between adjacent
  forecast times.
- **Vertical interpolation** — pressure / height interpolation across
  hybrid levels.
- **Time-series generation** — produce full series at a point or
  along a path.
- **Grid extraction** — whole-grid value vectors plus per-message
  metadata.
- **Thread-safe pooling** — `Model` pools `NFmiFastQueryInfo`
  instances so each thread gets its own iterator while sharing the
  underlying `NFmiQueryData`.

## 3. File ingestion

- **Directory monitor** — `Fmi::DirectoryMonitor` watches each
  producer's directory.
- **Regex file matching** — per-producer `pattern` controls which
  files are loaded.
- **Memory mapping** — `mmap=true` (default) maps files lazily;
  `mmap=false` loads into RAM.
- **Multi-threaded startup load** — `maxthreads = N` parallelises
  the initial scan.
- **`refresh_interval_secs`** — how often the watcher rescans.
- **`update_interval`** — minimum time between model updates per
  producer.
- **`number_to_keep`** — newest N files retained per producer.
- **`max_age`** — older models are pruned automatically
  (ISO duration, e.g. `"PT24H"`).
- **`minimum_expires`** — minimum lifetime for newly added models.

## 4. Producer configuration

Each producer is configured in the libconfig file:

- **`directory`** — input directory.
- **`pattern`** — regex selecting `.sqd` files.
- **`leveltype`** — pressure / height / hybrid / etc.
- **`type`** — producer type tag.
- **`forecast_type`** / **`forecast_number`** — for ensemble members.
- **Per-host overrides** — `overrides:( … )` lets a single config
  swap directories / patterns based on the host running the server.
- **Hot-reload** — `EngineImpl`'s `configFileWatcher` thread builds
  a new `RepoManager` when the config file changes and swaps it in
  atomically.

## 5. Repository & model lifecycle

- **`Model`** — represents one loaded `.sqd` file:
  - Owns `NFmiQueryData` lifetime.
  - Pools `NFmiFastQueryInfo` instances per thread.
  - Tracks origin time, expiration time, file path.
  - Factory-method creation (constructors private).
- **`Repository`** — `map<Producer, map<OriginTime, SharedModel>>`:
  - Producer lookup by name.
  - Best-spatial-match selection by coordinate.
  - Origin-time queries (latest, by index, by exact time).
  - Content / metadata reporting.
- **`RepoManager`** — owns the `Repository`, the directory monitors,
  and the expiration / pruning thread.
- **Atomic config reload** — `Fmi::AtomicSharedPtr<RepoManager>`
  ensures readers always see a consistent snapshot.

## 6. Automatic producer selection

- **By coordinate** — when no explicit producer is asked for, the
  engine picks the best matching producer whose geometry covers the
  requested point / area.
- **Spatial fallback** — if the top producer doesn't cover the
  request, the next-best is tried.
- **Producer ordering** — controlled via the config (priority order)
  plus per-producer geometry extents.

## 7. Metadata API

- **`MetaData`** — per-model metadata snapshot.
- **`MetaQueryOptions`** — filter options for engine metadata
  queries (producer, time range, parameter set).
- **`MetaQueryFilters`** — filter primitives.
- **`OriginTime`** — origin / analysis time abstraction.
- **`ValidTimeList`** — list of valid forecast times.
- **`ParameterOptions`** — parameter-fetch options.

## 8. Coordinate / spatial helpers

- **`Envelope`** — bounding-box helpers in lat/lon.
- **`WGS84EnvelopeFactory`** — build WGS84 envelopes from any input
  CRS.
- **`Range`** — numeric range type used by interpolators.
- **Spatial-reference fetching** — works with arbitrary GDAL CRS via
  `getWorldCoordinatesForSR`.

## 9. Caching

Two LRU caches inside `EngineImpl`:

- **`CoordinateCache`** — projected grid coordinates keyed by hash.
  Sized via `cache.coordinates_size` (default 100).
- **`ValuesCache`** — interpolated grid values keyed by hash. Sized
  via `cache.values_size` (default 5000).
- **`cache.lat_lon_size`** — latlon grid cache size (default 500).
- **`valid_points_cache_dir`** — filesystem cache for per-grid
  valid-point bitmaps; `clean_valid_points_cache_dir` cleans it on
  startup.

## 10. DEM-based correction

- **Temperature correction** — high-resolution DEM is used to adjust
  temperature values for sub-grid elevation differences.
- **Land/sea correction** — DEM + land cover used together to refine
  near-shore values.

## 11. Concurrency

- **Pooled `NFmiFastQueryInfo`** — each thread gets a private
  iterator instance; the underlying `NFmiQueryData` is shared
  read-only.
- **Background threads**:
  - Directory monitor (one per producer or shared).
  - Expiration / pruning thread.
  - Config-file watcher.
- **Atomic snapshot swap** — clients always see a consistent
  `Repository` even during reload.

## 12. Configuration knobs

Generic settings (in the libconfig main file):

- **`verbose`** — report newly loaded data.
- **`maxthreads`** — startup load parallelism.
- **`valid_points_cache_dir`** / **`clean_valid_points_cache_dir`**.
- **`cache.values_size`**, **`cache.coordinates_size`**,
  **`cache.lat_lon_size`**.

Per-producer (within `producers:( … )`):

- **`directory`**, **`pattern`**, **`pattern_str`**.
- **`leveltype`**, **`type`**.
- **`refresh_interval_secs`**, **`update_interval`**,
  **`number_to_keep`**, **`max_age`**, **`minimum_expires`**.
- **`mmap`**.
- **`forecast_type`**, **`forecast_number`**.

Per-host overrides via the `overrides:( … )` group.

## 13. Examples

Under `examples/` (build standalone with `make` inside `examples/`):

- **`QueryDataTest.cpp`** — direct querydata access demo.
- **`SmartmetTest.cpp`** — exercises the full engine via a SmartMet
  reactor.
- **`StackAllocationTest.cpp`** — internal allocation experiment.
- **Sample configs** — `querydata.conf` and `smartmet.conf`.

## 14. Documentation

- **`docs/docker.md`** — running the engine in Docker.

## 15. Build & integration

- **Output**: `querydata.so`.
- **Loaded at**: `$(prefix)/share/smartmet/engines/querydata.so`.
- **Build**: `make`.
- **Format**: `make format` runs clang-format (Google-based, Allman
  braces, 100-col).
- **Install**: `make install` — headers under
  `$(includedir)/smartmet/engines/querydata/`, library under
  `$(enginedir)`.
- **RPM**: `make rpm` (cleans first).
- **No automated test target** — `examples/` programs are run
  manually as smoke tests.
- **Linked libraries**: `smartmet-library-newbase`,
  `smartmet-library-spine`, `smartmet-library-gis`,
  `smartmet-library-macgyver`, `smartmet-library-timeseries`.
- **External libraries**: libconfig, GDAL, Boost (regex, thread,
  iostreams, serialization), jsoncpp, fmt.
- **CI**: CircleCI on RHEL 8 / RHEL 10 via the
  `fmidev/smartmet-cibase-{8,10}` Docker images and the standard
  `ci-build` workflow.

---

*Last updated: 2026-06-01.*
