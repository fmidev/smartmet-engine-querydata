# Querydata engine developer guide

This guide is for developers who change `smartmet-engine-querydata` (the "qengine"), or
write plugins that use it. It explains how data files become models, how plugins get and
use data, how producers are chosen for a location, how hash values and expiration times
are produced, the caches, and the rules for keeping the plugin interface compatible.

Related documents:

* [CLAUDE.md](../CLAUDE.md): architecture summary.
* [FEATURES.md](../FEATURES.md): the feature inventory.
* [radar.md](radar.md): radar GeoTIFF / ODIM HDF5 producers and the scratch cache.
* [smartsymbol.md](smartsymbol.md), [docker.md](docker.md).
* The newbase library's [querydata.md](https://github.com/fmidev/smartmet-library-newbase/blob/master/docs/querydata.md):
  the file format itself.
* The spine [developer guide](https://github.com/fmidev/smartmet-library-spine/blob/master/docs/developer-guide.md)
  (engine lifecycle, binary compatibility).

## Contents

1. [What the engine does](#1-what-the-engine-does)
2. [Building and testing](#2-building-and-testing)
3. [Source files](#3-source-files)
4. [From files to models](#4-from-files-to-models)
5. [The plugin API](#5-the-plugin-api)
6. [Choosing a producer for a location](#6-choosing-a-producer-for-a-location)
7. [Hash values and expiration times](#7-hash-values-and-expiration-times)
8. [Caches](#8-caches)
9. [Configuration](#9-configuration)
10. [Configuration reload](#10-configuration-reload)
11. [Binary compatibility](#11-binary-compatibility)
12. [Known pitfalls](#12-known-pitfalls)

---

## 1. What the engine does

The engine makes FMI querydata files (and, as lazy producers, radar GeoTIFF and ODIM
HDF5 files) available to the plugins:

* it watches the configured directories and loads new files as **models**, usually by
  memory-mapping them;
* it keeps the latest `number_to_keep` models (model runs) per **producer** and drops
  old ones;
* it hands plugins **`Q`** handles, through which they read, interpolate and iterate the
  data;
* it chooses the right producer for a location when a request does not name one;
* it gives plugins **hash values** and **expiration times** for the data, which the
  plugins use for ETags and `Expires` headers;
* it caches projected coordinates and grid values for repeated map requests.

Its users include the timeseries, wms, edr, download, download-radar, cross_section,
trajectory, autocomplete and meta plugins, and the contour engine.

## 2. Building and testing

```bash
make            # querydata.so
make install    # headers -> $(includedir)/smartmet/engines/querydata/, .so -> $(enginedir)
make format
cd examples && make && make test   # manual test programs; need a configuration
```

There are no automated tests in the repository; the engine is exercised by the test
suites of the plugins that use it (timeseries, wms, download, …) against the querydata
files in `smartmet-test-data`.

## 3. Source files

| File | Role |
|------|------|
| `Engine.{h,cpp}` | The plugin-facing base class. Every method is virtual and throws "engine is disabled"; loading the engine without configuration gives this class. |
| `EngineImpl.{h,cpp}` | The real engine: owns the `RepoManager` (through `Fmi::AtomicSharedPtr`), the caches, and the configuration-file watcher. |
| `RepoManager.{h,cpp}` | Reads the configuration, runs the `DirectoryMonitor` and the loader threads, removes expired models. |
| `Repository.{h,cpp}` | All loaded models, `map<Producer, map<OriginTime, SharedModel>>`: selection by producer, origin time or time period, producer search by location, content and metadata tables. |
| `Model.{h,cpp}` | One loaded file: the `NFmiQueryData`, a pool of `NFmiFastQueryInfo` objects, origin and modification time, hash value, expiration time. |
| `Q.{h,cpp}` | `QImpl`, the data handle given to plugins (a `shared_ptr`). |
| `Producer.{h,cpp}` | `ProducerConfig` and its parsing. |
| `MetaData`, `MetaQueryOptions`, `MetaQueryFilters` | Structured metadata for the metadata endpoints (EDR collections, …). |
| `Envelope`, `WGS84EnvelopeFactory`, `Range`, `ValidTimeList`, `OriginTime` | Helpers for metadata and time lists. |
| `RadarReader`, `RadarCatalog`, `RadarCache`, `Hdf5File` | Lazy radar producers (see [radar.md](radar.md)). |

## 4. From files to models

1. `RepoManager` starts one `DirectoryMonitor` watch per producer on `directory`,
   matching `pattern`, rescanned every `refresh_interval_secs` (60 s), for `CREATE`,
   `DELETE` and `SCAN` events.
2. `RepoManager::update()` receives new, changed and removed files. New files are loaded
   by loader threads into `Model` objects: memory-mapped when `mmap = true` (the default),
   otherwise read into memory. The model records its path, modification time and the
   producer's flags (`climatology`, `fullgrid`, `staticgrid`, `relative_uv`, …).
3. The model is added to the `Repository` under its producer and **origin time**. If a
   model with the same origin time exists, the file with the later modification time wins.
4. Old models are dropped so that at most `number_to_keep` (2) remain. `max_age` removes
   models older than the given age. `max_latest_age` excludes a producer from the
   location search (§6) while its **latest** model is older than the given age.
5. A `Q` handle holds a `shared_ptr` to its model, so a dropped model stays alive (and
   mapped) until the last plugin request using it has finished.

**Multifile producers** (`multifile = true`) treat all files as one data set, for example
observations split into daily files. `get(producer)` then returns a handle over **all**
models instead of the latest one.

**Lazy producers** (`lazy = true`, used for radar) only catalogue their files cheaply at
start; the data is decoded into a scratch cache on first access (`ensureLoaded()`).

## 5. The plugin API

Plugins get the engine with `reactor->getEngine<Engine::Querydata::Engine>("querydata")`.
The main calls:

| Call | Returns |
|------|---------|
| `producers()`, `hasProducer(p)`, `origintimes(p)`, `getProducerTimePeriod(p)`, `getProducerConfig(p)` | What is loaded. |
| `find(lon, lat, maxdist, usedatamaxdist, leveltype)`, `find(producerlist, …)` | The producer to use for a location (§6). |
| `get(p)`, `get(p, origintime)`, `get(p, timeperiod)` | A `Q` for the latest model, a given model run, or the models covering a period. Throws if there is no data. |
| `getModelHashValue(p[, origintime | timeperiod])` | Hash and expiration time of what `get()` with the same arguments would return (§7). |
| `getWorldCoordinatesDefault(q)`, `getWorldCoordinatesForSR(q, sr)`, `getValuesDefault(q, …)`, `getValuesForParam(q, …)` | Cached projected coordinates and grid values (§8). |
| `getProducerInfo()`, `getParameterInfo()`, `getEngineContents*()`, `getEngineMetadata*()` | Admin tables and structured metadata. |
| `getRadarLayerMetaData(p)` | Lazy radar producers. |

A **`Q`** wraps `NFmiFastQueryInfo` with thread-safe pooling: each `Q` has its own info
object, so plugins may iterate and interpolate freely. It provides parameter, level, time
and location iteration, point values, spatial interpolation (bilinear and others), time
interpolation, pressure and height interpolation, time-series generation at points and
areas, grid extraction, and the model's metadata (origin time, area, grid, levels,
parameters, hash value, expiration time).

## 6. Choosing a producer for a location

`find()` walks a list of producers **in order** (the configured order, or the list the
caller gives, where aliases are resolved) and returns the first whose latest model
**contains** the point for the requested level type:

* the point must be inside the model's area, or within `maxdist` km of a grid point with
  valid data;
* with `usedatamaxdist`, the producer's own `maxdistance` (if set) replaces the caller's
  `maxdist`;
* producers whose level type does not match are skipped, and so are producers whose
  latest model is older than their `max_latest_age`.

This is how a timeseries request without `producer=` picks the finest-resolution model
that covers the location: list the producers from the smallest area to the largest.

## 7. Hash values and expiration times

A model's **hash value** combines its file path, modification time and the producer flags.
`getModelHashValue()` returns the hash of exactly what `get()` with the same arguments
would select: the latest model, or for a multifile producer all models (combined). It
also returns their **expiration time**, and it loads a lazy producer first, because
callers (WMS ETag computation) ask for the hash before calling `get()`.

A model's expiration time is

```
max(modification time + update_interval, now + minimum_expires)
```

that is, "when the next model run is expected", but never less than `minimum_expires`
(600 s) from now. Plugins put it into `Expires` headers, and the frontend's cache
refreshes `Expires` from it.

## 8. Caches

`EngineImpl` keeps LRU caches, sized in the configuration:

| Cache | Key | Content |
|-------|-----|---------|
| `cache.coordinates_size` | hash of the model's grid and the target spatial reference | Projected coordinate matrices (`getWorldCoordinates*`); the data's own projection is not cached separately. |
| `cache.values_size` | hash computed by the caller of `getValues*` | Grid values. |
| `cache.lat_lon_size` | hash of the grid (in `RepoManager`) | Lat/lon coordinates of the grid points, shared by models on the same grid. |
| `radar.cache_size`, `radar.scratch_directory`, `radar.idle_timeout` | | The radar scratch cache ([radar.md](radar.md)). |

Their statistics are reported through `getCacheStats()` (`/admin?what=cachestats`).

## 9. Configuration

The configuration has a `producers` list, which also sets the default search order, and
one group per producer:

| Key | Default | Meaning |
|-----|---------|---------|
| `alias` | | Other names for the producer. |
| `directory`, `pattern` | | Where the files are, and a regex for their names. |
| `type`, `leveltype` | `grid`, `surface` | Data type and level type (for `find()`). |
| `refresh_interval_secs` | 60 | Directory rescan interval. |
| `number_to_keep` | 2 | Model runs kept. Keep at least 2: with 1, backends switching models at different times have no common content. |
| `update_interval` | 3600 | Expected interval between model runs (expiration time). |
| `minimum_expires` | 600 | Minimum expiration time from now. |
| `max_age`, `max_latest_age` | 0 (off) | Drop old models; hide a producer whose latest model is too old. |
| `maxdistance` | -1 | Producer-specific search distance for `find()`. |
| `multifile` | false | Treat all files as one data set. |
| `lazy` | false | Lazy (radar) producer. |
| `forecast`, `climatology` | true, false | Data kind. |
| `fullgrid`, `staticgrid` | true, false | Whether all grid points always have valid values; whether the valid points stay the same (used for masking). |
| `relative_uv` | false | U/V wind components are relative to the grid orientation. |
| `mmap` | true | Memory-map instead of reading into memory. |

Top-level `verbose`, the `cache.*` and `radar.*` settings, and host-specific
`overrides` (a list of `{ name = [hosts]; setting = value; }` groups) complete it.

## 10. Configuration reload

A watcher thread checks the configuration file's modification time. When it changes, a
completely new `RepoManager` is built from the new configuration, and it is swapped in
atomically once it reports ready (all its producers loaded). Requests already running keep the old manager (and its
models) alive through their `shared_ptr`s. During the reload, memory use roughly doubles,
because both managers map their files.

## 11. Binary compatibility

Plugins call the engine **through the virtual functions of `Engine`**, with the vtable
layout they were compiled against. Therefore:

* **add new virtual methods at the end of `Engine`**, never in the middle, and never
  reorder or remove them;
* changing the layout of `Q` / `QImpl`, `ProducerConfig`, `MetaData` or other classes that
  plugins construct or access inline also requires rebuilding the plugins.

Getting this wrong does not fail at load time. The old plugin calls the wrong slot and
typically crashes in an unrelated place. (This happened when a method inserted before
`getProducerTimePeriod()` made an old download plugin call it through the slot of
`getProducerConfig()`.) Release the engine and the plugins that use it together, and bump
their `Requires:`.

## 12. Known pitfalls

* **Early model runs are served with late expiration times.** The expiration time assumes
  the next run arrives `update_interval` after the current file. A run that arrives early
  does not shorten the `Expires` already given out, so clients and caches may keep the
  older product until that time.
* **In-place rewrites are not noticed.** The directory watch is for `CREATE`, `DELETE`
  and `SCAN`, not `MODIFY`. Write new files under a temporary name and rename them into
  place, or give each run a new file name.
* **Producer order matters.** `find()` returns the first producer that contains the point.
  Adding a large-area producer early in the list hides the finer ones behind it.
* **`number_to_keep = 1`** makes backends disagree during the rescan interval (see the
  table above).
* **Lazy producers load on first use**, including on the first hash-value request, so the
  first request after a start can be slow.
* **Reload doubles memory use** for a while (§10).
* **Virtual method order is ABI** (§11).
