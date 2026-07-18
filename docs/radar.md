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

## Scratch cache cleanup

Decoded scratch `.sqd` files are cleaned by three mechanisms, in order of
responsibility:

1. **Steady state** — `Model::~Model` unlinks its own scratch when the model is
   evicted by `number_to_keep` / `max_age`. When a source frame is overwritten
   its scratch name changes (the name embeds the source mtime), the old model is
   evicted, and its scratch is unlinked. Steady-state disk use is therefore
   bounded to roughly `number_to_keep` frames per producer.
2. **Startup reclamation** — on engine start, before the first scan,
   `RepoManager::cleanupOrphanedRadarScratch()` deletes every file under the
   scratch directory. This runs once per process (not on config hot-reload,
   where the previous manager still owns live scratch). It recovers from a crash
   or kill, which skips the model destructors: at startup nothing is mapped, so
   every file present is provably a stale orphan. Re-decoding the current frames
   costs well under a second, so a full reclaim is preferred over trying to
   preserve still-valid frames for a warm restart.
3. **External safety net (optional)** — because startup reclamation only runs
   when the process restarts, a host that is powered off with the server never
   restarting would retain orphans indefinitely. Ship a `tmpfiles.d` entry as a
   backstop, with an age far larger than any producer's `max_age` so it can
   never race a frame the server is still serving (e.g. during a stalled feed):

   ```
   # /etc/tmpfiles.d/smartmet-qengine-radar.conf
   d /var/tmp/smartmet-qengine-radar 0755 smartmet smartmet -
   e /var/tmp/smartmet-qengine-radar - - - 30d
   ```

Each server instance must use its own `radar.scratch_directory`; the startup
reclamation and the deterministic scratch names both assume the directory is
owned exclusively by one process.

## Testing

`examples/RadarReaderTest.cpp` decodes a committed synthetic EPSG:3067 GeoTIFF
fixture and a real Cartesian ODIM sample and checks dimensions, parameter,
valid time, gain/offset scaling, nodata/undetect handling, the north-up →
bottom-up row flip, and a scratch `.sqd` round-trip. Build and run it from the
`examples` directory: `make RadarReaderTest && ./RadarReaderTest`.
