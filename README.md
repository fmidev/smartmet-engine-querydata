Table of Contents
=================

  * [SMartMet Server](#SmartMet Server)
  * [Introduction](#introduction)
  * [Configuration](#configuration)
  * [Docker](#docker)

# SmartMet Server

[SmartMet Server](https://github.com/fmidev/smartmet-server) is a data
and procut server for MetOcean data. It provides high capacity and
high availability data and product server for MetOcean data. The
server is written in C++.

# Introduction 

In the SmartMet Server, the engines provide shared access to the data
and the plugins provide APIs based on the services provided by the
engines.

The querydata engine referred to as QEngine provides access to the grid
data. QEngine supports the QueryData format for data, but it has ready
tools to convert the data in GRIB, NetCDFand HDF format to QueryData.

QEngine memory-maps the data from NFS. It supports both spatial and
temporal interpolation and nearest point selection. The used method
depends on the parameter. QEngine selects the best data source for the
requested region.

QEngine has several Post-processing capabilities. It can correct the
data based on accurate DEM (up to 30 meter resolution) and also based
on land/water information.  Correlation done to the temperature is
based on the difference between model and real taking into account of
the following factors, namely, topography, land/water information,
used to give more weight on corresponding grid points in
interpolation. QEngine can calculate derivative parameters such as
FeelsLike, sunset, day length, etc.

# Configuration

## Generic settings

* `verbose = true/false` - in verbose mode the engine will report newly loaded data
* `maxthreads = N` - the number of threads used to read data on start up
* `valid_points_cache_dir = "path"` - directory where to cache information on the grids

## Overriding generic settings

Settings can be overridden for groups of hosts using an `overrides` group. Sample configuration:

```
overrides:
(
   {
      name = [ "test1", "test2.fmi" ];  # host name prefixes
      maxthreads = 5;                   # do not use too many threads on test machines
      verbose    = true;                # report what is being done on test machines
   },
   {
      name = [ "super1", "super2" ];
      maxthreads = 50;                  # use more threads on powerful servers
   }
)
```

## Producers

The `producers` setting will list the producers in the order they will be used for finding a producer for the requested coordinates if no producer is otherwise set in the request. Producers may be grouped using an `alias` command to limit the search.

Sample configuration:
```
producers =
[
    "local_default_model",   # local high resolution model
    "hirlam_europe",
    "ecmwf_world",           # this will match any coordinate
    ....                     # and hence the remaining producers must be requested explicitly by name
]
```

Each producer is configured in more detail using a top level block as follows:
```
hirlam_scandinavia:
{
        mmap                    = false;
        alias                   = "hirlam";
        directory               = "/path/to/hirlam/scandinavia/querydata";
        pattern                 = ".*_hirlam_scandinavia\.sqd$";
        forecast                = true;
        type                    = "grid";
        leveltype               = "surface";
        fullgrid                = false;
        refresh_interval_secs   = 45;
        number_to_keep          = 2;
        update_interval         = "PT1H";
        minimum_expires         = "PT10M";
        relative_uv             = false;
};
```

The individual settings are as follows
* `alias` - optional grouping for the producer
* `directory` - path to the querydata files
* `pattern` - required filename pattern
* `forecast` - true for forecasts, false for observations
* `climatology` - false by default. If true, the data can be queried for any year, only the date part matters
* `type` - grid, points. Some operations are permitted only for grids or points.
* `leveltype` - surface, pressure, model, points
* `fullgrid` - true if data is valid for all points, saves speed when server starts
* `refresh_interval_secs` - (default: 60) How often to check the directory for changes
* `number_to_keep` - (default: 2) How many newest models to keep in the engine. Should be at least two for directories that change over time in server clusters. If the data is static and never updates, the value can be set to 1. When using "multifile mode" this value should be greater than two, or if old data is otherwise often requested using origintime-settings.
* `update_interval` - (default: 3600) Estimated update interval for the data, used for expiration headers
* `minimum_expires` - (default: 600) Minimum expiration header even though the model might be just a minute or two late
* `mmap` - true by default, often set to false for the most important local model
* `max_age` - time when the data should be dropped from the engine even if it still exists on the disk
* `relative_uv` - are wind U- and V-components relative to the local grid orientation or east/north components

For historical reasons durations can be specified using ISO8601 or as simple offsets:
* 0, 0m, 0h (zero offset with or without units)
* (+|-){N}{unit} where N is a positive integer and unit is one of 'm' (minutes), 'h' (hours), 'w' (weeks) or 'y' (years). If the units are omitted, minutes will be used.

## Docker

SmartMet Server can be dockerized. This [tutorial](docs/docker.md)
explains how to explains how to configure the querydata engine
(QEngine) of the SmartMet Server when using Docker.
