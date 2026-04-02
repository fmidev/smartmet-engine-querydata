# smartmet-engine-querydata

Part of [SmartMet Server](https://github.com/fmidev/smartmet-server). See the [SmartMet Server documentation](https://github.com/fmidev/smartmet-server) for a full overview of the ecosystem.

## Overview

The querydata engine (QEngine) provides shared access to gridded weather forecast data in QueryData format. It memory-maps data from NFS, supports spatial and temporal interpolation, DEM-based temperature correction, and automatic selection of the best data source for a requested region.

## Configuration

### Generic settings

* `verbose = true/false` - in verbose mode the engine will report newly loaded data
* `maxthreads = N` - the number of threads used to read data on start up
* `valid_points_cache_dir = "path"` - directory where to cache information on the grids
* `clean_valid_points_cache_dir = true/false` - whether to automatically clean the above directory on start up or not

### Cache settings

* `cache.values_size = N` - how many processed grids to cache, default is 5000
* `cache.coordinates_size = N` - how many projected grid coordinates to cache, default is 100
* `cache.lat_lon_size = N` - how many latlon grids to cache, default is 500

### Overriding generic settings

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

### Disabling engine

QEngine can be loaded but disabled when only required to satisfy externel symbols in plugins, but not actually used
due to plugin configuration. There are several ways to specify, that QEngine is disabled:
* by providing configuration parameter `disabled = true`
* by not providing configuration at all
** setting  `configfile` is missing in engine section
** empty string is provided as the value of the `configfile` setting in engine section
A log message is generated in case of disabled engine

### Producers

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

### Translations for specific weather parameters

The Querydata Engine enables converting a numeric WeatherSymbol3 to a textual WeatherText parameter on the fly. The code assumes the translations are defined in the configuration for specific ISO2 language codes as follows:

```
language = "fi";

translations:
{
        WeatherText:
        (
                {
                        value   = 1;
                        en      = "sunny";
                        sv      = "klart";
                        fi      = "selke\\u00e4\\u00e4";
                },
                {
                        value   = 2;
                        en      = "partly cloudy";
                        sv      = "halvklart";
                        fi      = "puolipilvist\\u00e4";
                },
                {
                        value   = 3;
                        en      = "cloudy";
                        sv      = "mulet";
                        fi      = "pilvist\\u00e4";
                },
                ...
```

Unfortunately the translations for the more modern SmartSymbol are still hardcoded into the C++ code.

### Docker

SmartMet Server can be dockerized. This [tutorial](docs/docker.md)
explains how to explains how to configure the querydata engine
(QEngine) of the SmartMet Server when using Docker.

## License

MIT — see [LICENSE](LICENSE)

## Contributing

Bug reports and pull requests are welcome on [GitHub](../../issues).
