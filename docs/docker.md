# Tutorial

This tutorial explains how to configure the querydata engine (QEngine) of the SmartMet Server when using Docker.

## Prereqs

Docker software has been installed on some Linux server where you have access to already and the smartmetserver docker container is up and running.

### File querydata.conf

The purpose of the querydata configuration file "querydata.conf" is to define access to the grid data.

If you followed the “SmartMet Server Tutorial (Docker)” you have your configuration folders and files in the host machine at $HOME/docker-smartmetserver/smartmetconf but inside Docker they show up under /etc/smartmet. 

1. Go to the correct directory and enter command below to review the file:

```
$ less querydata.conf
```
You will see something like this:
```
verbose = true;

# Note: order is significant
producers =
[
        "hirlam_europe_surface",
        "hirlam_europe_pressure"
];

// types: grid, points
// leveltypes: surface, pressure, model

hirlam_europe_surface:
{
        alias                   = "hirlam";
        directory               = "/smartmet/data/hirlam/eurooppa/pinta/querydata";
        pattern                 = ".*_hirlam_eurooppa_pinta\.sqd$";
        forecast                = true;
        type                    = "grid";
        leveltype               = "surface";
        refresh_interval_secs   = 60;
        number_to_keep          = 4;
        multifile               = true;
};

hirlam_europe_pressure:
{
        alias                   = "hirlam_pressure";
        directory               = "/smartmet/data/hirlam/pressure";
        pattern                 = ".*_hirlam_europe_pressure\.sqd$";
        forecast                = true;
        type                    = "grid";
        leveltype               = "pressure";
        refresh_interval_secs   = 60;
        number_to_keep          = 2;
};

etc...

```
2. Use Nano or some other editor to enable more **producers** or to fix their directory paths if needed.

Some details and possible values of the producer:

```
//  types: grid, points
//  leveltypes: surface, pressure, model
//  number_to_keep: Number of files to keep for a producer, for example 2 
//  refresh_interval_secs: Refresh interval for a producer in seconds, for example 60
```

3. You can also give value for **maxthreads** that is used to define how many threads the engine can use simultaneously for loading querydata files. By limiting the number of threads, CPU time is given also for the other engines while memory-mapping new querydata.

```
maxthreads = n;
```
4. Configuring **synchronization (optional)** is useful if there are many backends. It defines settings querydata-engines use to keep their content in sync with others.

```
//Synchronization configuration (optional)

synchro:
  {
       port            = portaddress;
       hostname        ="host"  //Hostname is optional
 };
```
5. You can test querydata by query below that can be used to obtain the information about the currently loaded querydata:
```
http://hostname:8080/admin?what=qengine
```
You should see something like this:

![](https://github.com/fmidev/smartmet-plugin-wms/wiki/images/QengineData.PNG)

**Note:** Replace hostname with your host machine name, by localhost or by host-ip. This depends on where you have the container you are using.