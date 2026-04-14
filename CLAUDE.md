# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The **querydata engine** (QEngine) for SmartMet Server. It provides shared access to gridded weather forecast data in FMI's QueryData format. The engine memory-maps data files from disk/NFS, supports spatial and temporal interpolation, DEM-based temperature correction, and automatic producer selection for a requested geographic region. It is loaded as a shared object (`querydata.so`) by the SmartMet Server daemon at runtime.

## Build commands

```bash
make                # Build querydata.so
make clean          # Clean build artifacts
make format         # Run clang-format on all source and example files
make rpm            # Build RPM package (cleans first)
make install        # Install headers to $(includedir)/smartmet/engines/querydata/ and .so to $(enginedir)
```

There are no automated tests (`make test` exits with a message that tests are unautomated). The `examples/` directory contains standalone test programs (`QueryDataTest.cpp`, `SmartmetTest.cpp`, `StackAllocationTest.cpp`) that can be built and run manually:

```bash
cd examples && make          # Build example test executables
cd examples && make test     # Build and run them (requires querydata.so and a config)
```

## Architecture

### Two-class engine pattern (Engine / EngineImpl)

The engine uses a base class + implementation split to support a "disabled" mode:

- **`Engine`** (base) — inherits `Spine::SmartMetEngine`. All virtual methods throw "engine is disabled" by default. This allows the engine to be loaded but non-functional when not configured.
- **`EngineImpl`** (final) — the real implementation. Created via `EngineImpl::create(configfile)` factory method. Overrides all virtuals with actual logic.

Plugins obtain a pointer to the base `Engine` class from the server reactor and call its virtual API. They never know about `EngineImpl`.

### Core class hierarchy

- **`Q`** (`std::shared_ptr<QImpl>`) — the primary data access handle given to plugins. Wraps `NFmiFastQueryInfo` (from newbase) with safe memory management. Provides interpolation (spatial, temporal, pressure, height), time series generation, grid value extraction, and parameter iteration. All data access by plugins goes through Q objects.
- **`Model`** — represents a single loaded querydata file. Manages `NFmiQueryData` lifetime, pools `NFmiFastQueryInfo` instances for thread safety, and tracks origin time, expiration, and file path. Created via static factory methods (constructors are private via dummy `Private` struct).
- **`Repository`** — in-memory store of all loaded models, organized as `map<Producer, map<OriginTime, SharedModel>>`. Handles producer lookup by coordinate (finding the best spatial match), origin time queries, and content/metadata reporting.
- **`RepoManager`** — owns the `Repository` and manages background threads: `DirectoryMonitor` watches configured directories for new/changed querydata files, an expiration thread removes aged-out data. Parses the libconfig configuration file. On config reload, a new `RepoManager` is swapped in atomically via `Fmi::AtomicSharedPtr`.
- **`Producer` / `ProducerConfig`** — `Producer` is a `std::string` name. `ProducerConfig` holds all per-producer settings parsed from the config file (directory, pattern, refresh interval, number_to_keep, mmap flag, etc.).

### Data flow

1. `RepoManager` reads config, sets up `DirectoryMonitor` watchers per producer directory
2. When files appear/change, `RepoManager::update()` callback creates `Model` objects (memory-mapping or loading the querydata)
3. Models are inserted into `Repository`, old ones pruned by `number_to_keep` / `max_age`
4. Plugins call `Engine::get(producer)` -> returns a `Q` handle
5. Plugin uses `Q` for interpolation, time series, grid values etc.

### Config hot-reload

`EngineImpl` runs a `configFileWatcher` thread that monitors the config file's modification time. On change, it creates an entirely new `RepoManager` and atomically swaps it in. The old manager is held alive until its data is no longer referenced.

### Caching

Two LRU caches in `EngineImpl`:
- **CoordinateCache** — projected grid coordinates keyed by hash
- **ValuesCache** — interpolated grid values keyed by hash

Sizes configurable via `cache.coordinates_size` and `cache.values_size` in the config file.

## Key dependencies

- **newbase** (`NFmiQueryData`, `NFmiFastQueryInfo`) — the underlying querydata format library
- **spine** (`SmartMetEngine`, `Reactor`, `HTTP::Request`, `ParameterTranslations`) — server framework
- **gis** (`CoordinateMatrix`, `SpatialReference`, `CoordinateTransformation`) — coordinate operations
- **macgyver** (`DirectoryMonitor`, `Cache`, `AtomicSharedPtr`, `DateTime`) — utilities
- **timeseries** — time series data structures (`TS::Value`, `TS::TimeSeries*`)
- **libconfig** — configuration file parsing
- External: GDAL, Boost (regex, thread, iostreams, serialization), jsoncpp, fmt

## Code style

clang-format config: Google-based, Allman braces, 100-column limit, no bin-packing of arguments/parameters. Run `make format` before committing.

Namespace: `SmartMet::Engine::Querydata`. Source files live in `querydata/` subdirectory.

Two source files have `-Wno-deprecated-declarations` applied: `Engine.cpp` and `EngineImpl.cpp`.
