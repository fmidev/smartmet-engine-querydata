# Radar GeoTIFF / ODIM HDF5 support in the querydata engine

The querydata engine can serve monochrome radar rasters (FMI radar GeoTIFFs
and Cartesian ODIM HDF5) as ordinary querydata, so they drive isoband and
raster WMS layers with no plugin changes. This is the SmartMet-Server side of
migrating radar rendering off GeoServer.

## How it works

A producer whose files are GeoTIFF (`.tif`/`.tiff`) or ODIM HDF5 (`.h5`/`.hdf`)
is detected by file extension. On load, each source frame is decoded once into
a scratch `.sqd` and then memory-mapped through the normal querydata path:

- decode → `NFmiQueryData` (single parameter / level / time) on a native
  projection area (EPSG:3067 → `NFmiTransverseMercatorArea` when newbase
  ≥ 26.7.18 is installed; a PROJ-backed area otherwise),
- values scaled `gain*raw + offset`; `nodata → kFloatMissing`,
  `undetect → offset` (physical floor), matching `h5toqd`,
- the decoded scratch `.sqd` is written to the radar scratch directory; the
  model owns it and deletes it on eviction/expiry, and the kernel page cache
  manages the resident decoded frames (same model as the grid engine's GRIB
  `ValueCache`).

Everything downstream — `Q`, the contour engine, raster rendering, the
coordinate/value caches, and count/age eviction (`number_to_keep`, `max_age`) —
is unchanged.

## Configuring a radar producer

```
radar_finland_dbz:
{
    directory             = "/smartmet/radar/geotiff/radar_finland_dbz_3067";
    pattern               = ".*_radar_finland_dbz\.tif$";
    type                  = "grid";
    number_to_keep        = 50;   # also the animation window / scratch retention
    refresh_interval_secs = 60;
};
```

ODIM producers are identical but point at an `.h5` directory with an `.h5`
pattern. The valid time comes from the file (GeoTIFF `Observation time` metadata
or the `YYYYMMDDHHMM` filename stamp; ODIM `/what` date+time).

Engine-level option:

```
radar.scratch_directory = "/var/tmp/smartmet-qengine-radar";  # default; use a real-disk volume
```

## Limitations / TODO

- ODIM: only Cartesian objects (`COMP`/`IMAGE`/`CVOL`); polar volumes (`PVOL`)
  are rejected. Only the first data slice is read (single-quantity composites);
  multi-`dataset`/multi-`data` files (e.g. some accumulations) need extending.
- The ODIM quantity → newbase parameter map is a subset (DBZH/TH/DBZ, VRAD,
  WRAD, RATE, ACRR, ETOP); other quantities (RHOHV, ZDR, HCLASS, KDP) fall back
  to a synthetic parameter id and their quantity string as the name.
- All parameters use linear interpolation; categorical products (e.g. HCLASS)
  should use nearest-point.

## Scratch cache

Decoded scratch `.sqd` files live under `radar.scratch_directory` in one
subdirectory per source (producer): `<dir>/<producer>/<stem>_<sourcemtime>.sqd`.
The subdirectory is the unit of caching and eviction, because radar access is
source-local: when a source is used its whole timeseries is wanted, so a
per-file LRU would half-cache hot sources. The cache is managed by `RadarCache`.

### Cleanup mechanisms

1. **Steady state** — `Model::~Model` unlinks its own scratch when the model is
   evicted by `number_to_keep` / `max_age`. As a source frame rotates, its
   scratch name changes (the name embeds the source mtime), so per-source disk
   use tracks the source frame count.
2. **Startup reconcile** — before the first scan, `RepoManager::reconcileRadarCache()`
   reconciles the on-disk cache against the configured sources instead of wiping
   it: it removes crash residue (dot-prefixed temp files and any `.trash`), drops
   frames whose source has rotated away and sources no longer configured, keeps
   still-current frames so the restart is **warm**, and enforces the size budget.
   Runs once per process (not on config hot-reload). Crash-safe by construction:
   the tree is the source of truth, so a crash at any point converges on the next
   reconcile. See the crash-resistance notes below.
3. **Size budget** — `radar.cache_size` (bytes; `0` = unlimited) caps total cache
   size. When exceeded, whole least-recently-accessed sources are evicted down to
   a low-water mark. Recency is the mtime of a per-source `.accessed` marker
   (touched, rate-limited, on access — an atomic metadata write). A source is
   *pinned* (never evicted) while it has live models. **Caveat:** under the
   current eager per-producer load, every configured radar source has live models
   and is therefore pinned, so the budget currently reclaims only de-configured or
   expired sources. Making it a true working-set limiter (evicting cold *configured*
   sources and reloading them lazily on access) requires lazy per-source loading.

### Crash resistance

The cache needs atomicity, not durability — it is a pure function of the source
files, so anything lost is re-decoded. There is deliberately **no authoritative
index file**: correctness state is the tree itself (filenames + source mtimes),
recency is an atomic `.accessed` mtime, and eviction of a whole source renames its
subdir into `.trash` (atomic) before removal. Dot-prefixed temps and markers are
ignored by the newbase reader, so a half-written decode is never seen as data.

### Configuration

```
radar.scratch_directory = "/var/tmp/smartmet-qengine-radar";  # per instance
radar.cache_size        = 53687091200;  # 50 GiB budget; 0 = unlimited
```

Each server instance must use its own `radar.scratch_directory` (the naming and
eviction assume exclusive ownership).

## Testing

`examples/RadarReaderTest.cpp` decodes a committed synthetic EPSG:3067 GeoTIFF
fixture and a real Cartesian ODIM sample and checks dimensions, parameter,
valid time, gain/offset scaling, nodata/undetect handling, the north-up →
bottom-up row flip, and a scratch `.sqd` round-trip. Build and run it from the
`examples` directory: `make RadarReaderTest && ./RadarReaderTest`.
