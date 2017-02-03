%define DIRNAME querydata
%define LIBNAME smartmet-%{DIRNAME}
%define SPECNAME smartmet-engine-%{DIRNAME}
Summary: SmartMet qengine engine
Name: %{SPECNAME}
Version: 17.2.3
Release: 2%{?dist}.fmi
License: MIT
Group: SmartMet/Engines
URL: https://github.com/fmidev/smartmet-engine-querydata
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: boost-devel
BuildRequires: libconfig >= 1.4.9
BuildRequires: smartmet-library-spine-devel >= 17.1.24
BuildRequires: smartmet-library-newbase-devel >= 17.1.26
BuildRequires: smartmet-library-macgyver-devel >= 17.1.18
BuildRequires: protobuf-compiler
BuildRequires: protobuf-devel
BuildRequires: protobuf
Requires: boost-filesystem
Requires: boost-date-time
Requires: boost-iostreams
Requires: boost-regex
Requires: boost-thread
Requires: boost-system
Requires: smartmet-library-newbase >= 17.1.26
Requires: smartmet-library-macgyver >= 17.1.18
Requires: protobuf
Requires: libconfig >= 1.4.9
Requires: smartmet-library-spine >= 17.1.24
Provides: %{LIBNAME}
Obsoletes: smartmet-brainstorm-qengine < 16.11.1
Obsoletes: smartmet-brainstorm-qengine-debuginfo < 16.11.1

%description
SmartMet querydata engine

%package -n %{SPECNAME}-devel
Summary: SmartMet %{SPECNAME} development headers
Group: SmartMet/Development
Provides: %{SPECNAME}-devel
Obsoletes: smartmet-brainstorm-qengine-devel < 16.11.1
%description -n %{SPECNAME}-devel
SmartMet %{SPECNAME} development headers.

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n engines/%{DIRNAME}
 
%build -q -n engines/%{DIRNAME}
make %{_smp_mflags}

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files -n %{SPECNAME}
%defattr(0755,root,root,0775)
%{_datadir}/smartmet/engines/%{DIRNAME}.so

%files -n %{SPECNAME}-devel
%defattr(0664,root,root,0775)
%{_includedir}/smartmet/engines/%{DIRNAME}/*.h

%changelog
* Fri Feb  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.3-2.fmi
- Added parameter descriptions to repository report
- Report parameter number if the name of the parameter is not known
- Improved WKT output

* Fri Feb  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.3-1.fmi
- Admin-plugin now reports querydata level values too

* Thu Jan 26 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.26-1.fmi
- Added caching for NFmiQueryData object LatLonCache objects
- Optimized update algorithms to utilize the latlon cache

* Mon Jan  9 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.9-2.fmi
- The Model now initializes the LatLonCache to avoid race conditions later on

* Mon Jan  9 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.9-1.fmi
- Reduced write locking during repository updates to avoid race conditions

* Wed Jan  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.4-1.fmi
- Changed to use renamed SmartMet base libraries

* Wed Nov 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.30-1.fmi
- Removed open configuration
- No installation for configuration

* Tue Nov 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.29-1.fmi
- Added a WGS84Envelope data member to Q and MetaData classes.
- Simple Envelope class added which calculates WGS84 bounding box of a querydata.
- Simple Range class added to store minimum and maximum values.

* Fri Nov 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.18-1.fmi
- Added growth season history data

* Tue Nov 15 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.15-3.fmi
- Temporary hotfix for myocean requests

* Tue Nov 15 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.15-2.fmi
- Added myocean alias to new Copernicus data, removed old myocean data

* Tue Nov 15 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.15-1.fmi
- Added Copernicus marine forecasts

* Tue Nov  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.1-1.fmi
- Namespace changed

* Tue Oct  4 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.10.4-1.fmi
- stationname will now be none if the location is not a station

* Tue Sep 20 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.20-1.fmi
- Changed default OAAS producer from EC to HIRLAM

* Tue Sep  6 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.6-1.fmi
- New exception handler

* Mon Aug 15 2016 Markku Koskela <markku.koskela@fmi.fi> - 16.8.15-1.fmi
- Making sure that also the RepoManager and Syncro objects get a warning
- when the shutdown is requested.

* Thu Jun 30 2016  Santeri Oksman <santeri.oksman@fmi.fi> - 16.6.30-1.fmi
- Added EC probability data

* Wed Jun 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.29-1.fmi
- Added a maxdistance setting for producers
- Added a boolean to find() for specifying whether data specific maxdistance is to be used

* Wed Jun 22 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.22-1.fmi
- Keep only the last 5 global EC forecasts to prevent bus errors

* Tue Jun 21 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.21-1.fmi
- Added WAM model
- Added metsapaloindex_1km

* Tue Jun 14 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.14-1.fmi
- Updated WAFS path
- Added ECMWF weekly climatology data

* Thu Jun  2 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.2-1.fmi
- Full recompile

* Wed Jun  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.1-1.fmi
- Added graceful shutdown

* Wed May 11 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.5.11-1.fmi
- Added cache size reporting

* Wed May  4 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.5.4-1.fmi
- Fixed issue in landscaping functionality

* Wed Apr 20 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.4.20-1.fmi
- Removed coupling with Contour - engine

* Mon Apr 11 2016  Santeri Oksman <santeri.oksman@fmi.fi> - 16.4.11-1.fmi
- Keep 999 daily00 kriging files instead of 7

* Wed Mar 30 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.30-1.fmi
- Reintroduced global Myocean

* Tue Mar 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.3.29-1.fmi
- Added ECMWF global history -data for Kalman & MOS

* Wed Mar 23 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.23-1.fmi
- Once again brought back EC-world-kalman producer

* Tue Mar 22 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.22-1.fmi
- Reverted MyOcean back to arctic data because global data provision has issues

* Tue Mar 15 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.15-1.fmi
- Once again downgraded global Kalman due to data issues

* Fri Mar 11 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.11-1.fmi
- Added anomality kriging - producer

* Mon Mar  7 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.7-1.fmi
- Added EC-kalman producer to the 2nd place

* Fri Mar  4 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.4-1.fmi
- Gridded landscaping functionality added
- Removed obsolete mbehirlam models
- Renamed mbehirlam_suomi_mallipinta to hirlam_suomi_mallipinta
- Added alias mhehirlam_suomi_mallipinta for hirlam_suomi_mallipinta

* Thu Mar  3 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.3-1.fmi
- Producer 'myocean' now refers to the global data

* Tue Feb  9 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.9-1.fmi
- Rebuilt against the new TimeSeries::Value definition

* Tue Feb  2 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.2-1.fmi
- Now using Timeseries None - type

* Tue Jan 26 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.1.26-1.fmi
- Added ERA Finland test dataset

* Sat Jan 23 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.23-1.fmi
- Fmi::TimeZoneFactory API changed

* Thu Jan 21 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.21-1.fmi
- Added roadmodel for Finnish airports

* Tue Jan 19 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.19-1.fmi
- Added WAFS data source for flight level forecasts

* Mon Jan 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.18-1.fmi
- newbase API changed, full recompile

* Mon Jan 11 2016 Santeri Oksman <santeri.oksman@fmi.fi> - 16.1.11-1.fmi
- Move parameterIsArithmetic check from qengine to timeseries.

* Thu Dec  3 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.12.3-1.fmi
- Added seaice_analysis - producer

* Wed Nov 18 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.11.18-1.fmi
- Fixed Swedish translations of the FMI weather symbols

* Wed Nov  4 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.4-1.fmi
- Pass local_date_time instead of ptime to newbase for interpolation for speed and accuracy

* Tue Oct 27 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.10.27-1.fmi
- Added producer plowmodel_suomi_pinta
- Added producers ravake_suomi_eceps12h and -eceps24h
- Added producers kriging_suomi_synop, -snow, -daily06 and -daily18

* Mon Oct 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.10.26-1.fmi
- Added proper debuginfo packaging

* Mon Oct 12 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.10.12-1.fmi
- Pollen is now a multifile-producer

* Tue Sep 29 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.9.29-1.fmi
- Added covertype parameter

* Mon Sep 28 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.9.28-1.fmi
- Changed resampling to use NFmiGdalArea bounding box constructor for correctness

* Tue Sep 22 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.9.22-1.fmi
- Use an optimal resolution DEM when sampling querydata for improved speed

* Wed Sep 16 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.9.16-1.fmi
- Prevent a crash if one requests very low resolution data at high zoom levels

* Tue Sep  8 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.9.8-1.fmi
- Add wwi model to configuration.

* Wed Aug 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.26-1.fmi
- Recompiled with latest newbase with faster parameter changing

* Tue Aug 18 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.18-1.fmi
- Use time formatters from macgyver to avoid global locks from sstreams

* Mon Aug 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.17-2.fmi
- Use -fno-omit-frame-pointer to improve perf use

* Mon Aug 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.17-1.fmi
- Provide a method for directly querying the time period covered by a model

* Fri Aug 14 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.14-1.fmi
- Avoid boost::lexical_cast, Fmi::number_cast and std::ostringstream for speed

* Tue Aug  4 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.8.4-1.fmi
- Replaced sealevel-producer with EC-OAAS
- Removed lon-lat order trickery, it is now handled in table formatting

* Fri Jun 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.26-1.fmi
- Added weathersymbol metaparameter

* Tue Jun 23 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.23-1.fmi
- Recompiled since Location API changed

* Wed Jun 10 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.10-1.fmi
- FeelsLike, WindChill, ApparentTemperature etc now use landscape corrected temperature

* Mon Jun  8 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.8-1.fmi
- FeelsLike temperature now takes solar radiation into account

* Mon May 25 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.5.25-1.fmi
- Added 'ecmwf_maailma_kalman' producer and deprecated 'ecmwf_maailma_piste'
- Added 'lamposumma' producer
- Q object now recognizes lat and lon parameters
- Adjusted parameter type definitions for consistency

* Tue May 19 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.5.19-1.fmi
- Replaced old WAM producer with a better one

* Wed Apr 29 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.29-1.fmi
- Added ecmwf 24h producer
- Added lake information to landscape sampling

* Mon Apr 27 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.27-1.fmi
- qengine now requires origintime requests to be exact

* Fri Apr 24 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.4.24-1.fmi
- Changes introduced in 15.4.22-1.fmi were discarded due to snafu in previous release. Now re-introduced.

* Thu Apr 23 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.23-1.fmi
- Keep 10 days of ECMWF world forecasts for Kalman calculations

* Wed Apr 22 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.4.22-1.fmi
- Added configurations

* Wed Apr 15 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.15-1.fmi
- newbase API on NFmiQueryData::LatLonCache changed

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-3.fmi
- Sleep without locking other querydata threads

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-2.fmi
- Added a maxthreads setting for the number of querydata loading threads

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-1.fmi
- newbase API changed

* Wed Apr  8 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.8-1.fmi
- Dynamic linking of smartmet libraries
- Added sampling of querydata to higher resolutions

* Mon Mar 23 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.23-2.fmi
- Added metaparameter 'dem'
- Added metaparameter 'tdem'

* Mon Mar 23 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.23-1.fmi
- Brought back roadmodel_skandinavia - alias

* Tue Mar 17 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.17-1.fmi
- Adjusted pollen producer regex

* Fri Mar 13 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.13-1.fmi
- Roadmodel Scandinavia is now the default 'roadmodel'

* Tue Feb 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.24-1.fmi
- new feels-like temperature formula

* Wed Jan 14 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.1.14-1.fmi
- qengine origintime option is now an upper limit for the data origintime

* Tue Jan 13 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.1.13-1.fmi
- Added support for getting the oldest or latest model
- Added roadmodel_suomi_piste with alias roadmodel_piste

* Wed Dec 17 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.17-1.fmi
- Recompiled due to spine API changes

* Mon Dec 15 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.15-1.fmi
- Fixed the Repository coordinate finder not to lose NFmiFastQuery objects from the spool

* Fri Nov 14 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.11.14-1.fmi
- Fix sealevel data path also for open data machines

* Thu Nov 13 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.11.13-1.fmi
- Sealevel model changed to oaas_hirlam

* Tue Oct 28 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.28-1.fmi
- Added missing nan-handling in Snow1h{Upper,Lower}

* Wed Oct 15 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.15-2.fmi
- Ecmwf_maailma_pinta is now a multifile

* Wed Oct 15 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.15-1.fmi
- Ecmwf_maailma_pinta is now second only to pal_skandinavia

* Thu Oct  9 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.9-1.fmi
- Added myocean producer

* Tue Sep  2 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.2-1.fmi
- Fixed cache key generation for models

* Fri Aug 29 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.29-1.fmi
- Added hash_value functions to Model and Q objects

* Thu Aug 28 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.8.28-2.fmi
- Added harmonie and kriging producers

* Thu Aug 28 2014 Mikko Visa <mikko.visa@fmi.fi> - 14.8.28-1.fmi
- WAM datadir changed

* Wed Aug  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.6-1.fmi
- WKT for latlon and rotated latlon is fixed not to include PROJCS

* Fri Jul 18 2014 Santeri Oksman <santeri.oksman@fmi.fi> - 14.7.18-1.fmi
- Fixed pollen data pattern

* Mon Jun 30 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.6.30-1.fmi
- Recompiled with latest spine API

* Tue Jun 17 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.6.17-1.fmi
- Fixed issue in Q::Value() - method, it used to always look for nearestpoint even if data value was succesfully interpolated

* Tue May 27 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.5.27-1.fmi
- Added tuliset producer

* Wed May 14 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.14-2.fmi
- Use shared macgyver and locus libraries

* Wed May 14 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.5.14-1.fmi
- Hotfix release: removed ecwmf_maailma_piste from ec-alias due to borked data

* Thu May  8 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.8-1.fmi
- Fixed order of values in latlon queries

* Wed May  7 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.5.7-1.fmi
- Tweaked pollen producer entry in conf

* Tue May  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.6-1.fmi
- API refactoring by Anssi Reponen

* Mon Apr 28 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.4.28-1.fmi
- Full recompile due to large changes in spine etc APIs

* Thu Apr 10 2014 Anssi Reponen <anssi.reponen@fmi.fi> 14.4.10-1.fmi
- API modified: values-functions moved from Engine-class to QImpl-class
- Aggregation functionality moved to spine

* Fri Mar 28 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.3.28-1.fmi
- Added metsapalomalli producer

* Fri Mar 14 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.3.14-1.fmi
- Added climate_suomi_pisteet producer

* Thu Mar 13 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.3.13-1.fmi
- Added harmonie scandinavia model

* Thu Mar  6 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.3.6-1.fmi
- Changed ECMWF UV producer path to /smartmet/data/ecmwf/maailma/uv3h/querydata

* Fri Feb 28 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.2.28-1.fmi
- Added ECMWF World pressure level data configuration

* Thu Feb 27 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.2.27-1.fmi
- Corrected Swedish translations of the 'weather'-parameter
- Fixed thread counting mechanism to avoid too early exit from the init method

* Mon Feb 17 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.2.17-1.fmi
- Meta parameter names are now case insensitive too

* Wed Feb  5 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.2.5-1.fmi
- Added EC Sotshi data

* Mon Feb 3 2014 Mikko Visa <mikko.visa@fmi.fi> - 14.2.3-2.fmi
- Open data 2014-02-03 release
- Added time offset forwards for aggregation
- Added Integ function

* Tue Jan 28 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.1.28-1.fmi
- Added roadmodel_andorra - producer

* Mon Jan 13 2014 Santeri Oksman <santeri.oksman@fmi.fi> - 14.1.13-1.fmi
- Added water to snow conversion parameters Snow1h, Snow1hUpper, Snow1hLower

* Thu Dec 12 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.12.12-1.fmi
- New way to determine if parameter is aggregatable. 
- Added HELMI ice model into main configuration of the engine.
- Fixed HELMI ice model query data file name pattern configuration.

* Thu Nov 28 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.28-1.fmi
- More protection against corrupted querydata files

* Tue Nov 26 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.26-1.fmi
- Fixed to format negative geoid values normally

* Mon Nov 25 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.25-1.fmi
- Added metaparameters modtime and mtime, which are equivalent

* Tue Nov 19 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.11.19-1.fmi
- metadata-method in Q should work now

* Tue Nov 12 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.14-1.fmi
- If there are two models with the same origin time, choose the one with the newer modification time
- Fixed broken metadata retrieval

* Tue Nov 12 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.11.12-1.fmi
- Fixed broken metadata retrieval

* Thu Nov  7 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.7-1.fmi
- Q now provides means to track sub index status

* Tue Nov  5 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.5-1.fmi
- Parameters for moonrise and moonset
- QEngine is now initialized mostly in the init method to speed up concurrent server initialization
- Use some colour in error messages

* Wed Oct  9 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.10.9-2.fmi
- The configuration file now lists which models do not cover all grid points
- Speed up the detection of valid points in models which do not cover the entire grid

* Wed Oct  9 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.10.9-1.fmi
- Now conforming with the new Reactor initialization API
- get-function return value changed from ModelList to SharedModelList
- Added daylength parameter
- RepoManager now holds a lock for a shorter time, querydata gets published faster
- RepoManager now tracks how many threads are loading querydata

* Wed Oct  2 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.10.2-1.fmi
- Catch timeseries generating empty boost::any answers

* Mon Sep 23 2013 Tuomo Lauri    <tuomo.lauri@fmi.fi>    - 13.9.23-1.fmi
- Performance optimizations

* Mon Sep 16 2013 Anssi Reponen  <anssi.reponen@fmi.fi>    - 13.9.16-1.fmi
- Perfomance optimized

* Fri Sep 6  2013 Tuomo Lauri    <tuomo.lauri@fmi.fi>    - 13.9.6-1.fmi
- Recompiled due Spine changes

* Thu Sep 5 2013 Tuomo Lauri   <tuomo.lauri@fmi.fi>      - 13.9.5-1.fmi
- Added global ECMWF UV data to commercial configuration

* Thu Aug 29 2013 Anssi Reponen    <anssi.reponen@fmi.fi>    - 13.8.29-1.fmi
- Aggregation related fix in the handling of parameters whose value is independent of 
location inside area: country, iso2, region, tz, localtz, name, place, geoid, time, 
localtime, utctime, origintime, epochtime, isotime, xmltime

* Wed Aug 28 2013 Tuomo Lauri    <tuomo.lauri@fmi.fi>    - 13.8.28-1.fmi
- Aggregation related fixes

* Mon Aug 12 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.8.12-1.fmi
- Recompiled due to Pacific view changes in newbase

* Tue Jul 23 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.7.23-1.fmi
- Recompiled due to thread safety fixes in newbase & macgyver

* Wed Jul  3 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.7.3-1.fmi
- Update to boost 1.54

* Tue Jun 18 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.6.18-1.fmi
- Harmonized producer names between regular and open configs

* Mon Jun 17 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.6.17-1.fmi
- Aggregation of std::pair<double, double> type corrected.
- Return value for longitude, latitude, lonlat, latlon-parameter corrected for the area.

* Tue Jun  4 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.6.4-1.fmi
- Fixed model selection

* Mon Jun  3 2013 lauri <tuomo.lauri@fmi.fi> - 13.6.3-1.fmi
- Rebuilt against the new Spine

* Thu May 16 2013 lauri <tuomo.lauri@fmi.fi> - 13.5.16-1.fmi
- Rebuilt against new spine

* Mon Apr 29 2013 tervo <roope.tervo@fi.fi> - 13.4.29-1.fmi
- Changed hirlam and monthly data default names in open data configuration.

* Mon Apr 22 2013 mheiskan <mika.heiskanen@fi.fi> - 13.4.22-1.fmi
- Brainstorm API changed

* Wed Apr 17 2013 tervo <roope.tervo@fmi.fi>    - 13.4.17-1.fmi
- Added hmb and wav to open data configuration

* Fri Apr 12 2013 lauri <tuomo.lauri@fmi.fi>    - 13.4.12-1.fmi
- Built against the new Spine

* Fri Mar 15 2013 tervo <roope.tervo@fmi.fi> - 13.3.15-1.fmi
- Added separate configuration files for open data and commercial use.

* Thu Mar 14 2013 oksman <santeri.oksman@fmi.fi> - 13.3.14-1.fmi
- New build from develop branch

* Wed Mar  6 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.3.6-1.fmi
- Recompiled with newbase to get interpolation bug fix activated
- Made hostname optional in QEngine synchronization config

* Wed Feb  6 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.2.6-1.fmi
- Built against new Spine and Server

* Mon Nov 19 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.19-1.fmi
- Improved error message if given origintime is not available

* Thu Nov 15 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.15-1.el6.fmi
- Added EC world wave forecast

* Wed Nov  7 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.7-1.el6.fmi
- Upgrade to boost 1.52
- Refactored spine library into use

* Tue Sep 18 2012 lauri    <tuomo.lauri@fmi.fi>    - 12.9.18-1.el6.fmi
- Recompile due to changes in macgyver astronomy

* Thu Sep  6 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.9.6-1.el6.fmi
- Enabled MESAN data

* Tue Aug 14 2012 lauri  <tuomo.lauri@fmi.fi>    - 12.8.14-1.el6.fmi
- Qengine contents reporing now includes the following:
- Parameter names, model time spans, projection information

* Thu Aug  9 2012 oksman <santeri.oksman@fmi.fi> - 12.8.9-2.el6.fmi
- Prefer ecmwf_maailma before gfs_world.

* Thu Aug  9 2012 lauri    <tuomo.lauri@fmi.fi>    - 12.8.9-1.el6.fmi
- Added ability to query QEngine contents

* Wed Aug  8 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.8.8-1.el6.fmi
- Added parameter 'feature'

* Tue Aug 7  2012 lauri    <tuomo.lauri@fmi.fi>    - 12.8.7-1.el6.fmi
- Location API changed

* Wed Jul 25 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.25-1.el6.fmi
- FeelsLike now returns ApparentTemperature

* Mon Jul 23 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.23-1.el6.fmi
- Added ApparentTemperature

* Thu Jul  5 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.5-1.el6.fmi
- Upgrade to boost 1.50

* Tue Jul  3 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.3-1.el6.fmi
- Recompiled with latest server API
- Increased number_to_keep to 2 for all data sources until a reload bug is fixed

* Wed May 30 2012 oksman <santeri.oksman@fmi.fi> - 12.5.30-2.el6.fmi
- Added laps_suomi and laps_skandinavia models.

* Wed May 30 2012 oksman <santeri.oksman@fmi.fi> - 12.5.30-1.el6.fmi
- Removed eceps data as unnecessary.

* Mon May 14 2012 oksman <santeri.oksman@fmi.fi> - 12.5.14-1.el6.fmi
- Added Ravake probability forecast data to qengine.conf.

* Thu Apr  5 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.5-1.el5.fmi
- SummerSimmer index is fixed to use RH in range 0-1

* Wed Apr  4 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.4-1.el6.fmi
- Removed make_parameter, the code is now in brainstorm-common

* Mon Apr  2 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.2-1.el6.fmi
- macgyver change forced recompile

* Sat Mar 31 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.31-2.el5.fmi
- Added SummerSimmerIndex parameter with alias SSI
- Added FeelsLike parameter, which combines SSI with WindChill

* Sat Mar 31 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.31-1.el5.fmi
- Upgraded to boost 1.49

* Tue Mar 13 2012 oksman <santeri.oksman@fmi.fi> - 12.3.13-1.el5.fmi
- Changed mbehirlam file pattern to accept also *hirlam.

* Tue Jan 31 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.01.31-1.el5.fmi
- Added global ECMWF surface forecast

* Sat Jan 21 2012 tervo <roope.tervo@fmi.fi> - 12.1.21-1.el5.fmi
- Added snow accumalation forecast

* Tue Jan 10 2012 tervo <roope.tervo@fmi.fi> - 12.1.10-2.el5.fmi
- Repaired search pattern

* Tue Jan 10 2012 tervo <roope.tervo@fmi.fi> - 12.1.10-1.el5.fmi
- Added sealevel forecast

* Wed Dec 21 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.21-1.el6.fmi
- RHEL6 release

* Fri Nov  4 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.11.4-1.el5.fmi
- Default data is now the surface history file with 3 days of past data

* Tue Sep  6 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.9.6-1.el5.fmi
- Fixed thread safety issue

* Tue Aug 16 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.8.16-1.el5.fmi
- Upgrade to boost 1.47 and latest newbase

* Thu Jun 23 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.6.23-1.el5.fmi
- Fixed qengine to always be nonverbose by default

* Tue Mar 29 2011 tervo <roope.tervo@fmi.fi> - 11.3.29-2.el5.fmi
- Added mbehirlam_mallipinta

* Thu Mar 24 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.24-1.el5.fmi
- Upgrade to boost 1.36

* Wed Mar  9 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.9-1.el5.fmi
- Improved error handling in qengine

* Thu Mar  3 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.3-1.el5.fmi
- Always report if qengine fails to load a file

* Wed Mar  2 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.2-1.el5.fmi
- Added verbose mode

* Tue Jan 18 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.1.18-1.el5.fmi
- Refactored query string option parsing

* Wed Nov 17 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.11.17-1.el5.fmi
- Added Weather metaparameter

* Thu Oct 28 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.10.28-1.el5.fmi
- Added value extraction for pointforecast & autocomplete

* Thu Sep 16 2010 oksman <santeri.oksman@fmi.fi> - 10.9.16-1.el5.fmi
- Added daily00 and mesan data.

* Tue Sep 14 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.9.14-1.el5.fmi
- Upgrade to boost 1.44

* Mon Jul  5 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.7.5-1.el5.fmi
- Recompile brainstorm due to newbase hessaa bugfix

* Wed Jun 23 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.6.23-1.el5.fmi
- Added pal_skandiavia_frost data

* Tue Mar 23 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.3.23-1.el5.fmi
- Added ability to handle climatology data
- Added climatology data to the server

* Sat Feb 13 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.2.13-1.el5.fmi
- Fixed model ordering

* Fri Feb 12 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.2.12-1.el5.fmi
- Refactored code
- Added possibility to find nearest valid grid points

* Fri Jan 15 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.15-1.el5.fmi
- Upgrade to boost 1.41

* Tue Dec 15 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.12.15-1.el5.fmi
- Added support for multiple aliases for querydata

* Thu Nov 26 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.11.26-1.el5.fmi
- Added ECMWF fractile forecasts

* Thu Oct 29 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.10.29-1.el5.fmi
- Added hill forecasts to qengine.conf

* Wed Jul 22 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.7.22-1.el5.fmi
- Fixed code to be in SmartMet namespace

* Tue Jul 14 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.7.14-1.el5.fmi
- Upgrade to boost 1.39

* Thu Apr 16 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.4.16-1.el5.fmi
- Added EC UV forecast to qengine.conf

* Wed Mar 25 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.3.25-1.el5.fmi
- Changed PAL-data to pinta_xh

* Mon Feb 23 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.2.23-1.el5.fmi
- Added ECMWF wave model
- Recompiled with latest libraries

* Fri Dec 19 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.19-1.el5.fmi
- Compiled with latest newbase to get case independent parameter names

* Tue Dec 16 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.16-1.el5.fmi
- Added maxdistance parameter to model selection

* Mon Dec 15 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.15-1.el5.fmi
- Added gfs world kalman data to qengine.conf

* Wed Dec 10 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.10-1.el5.fmi
- API update for leveltype support

* Wed Nov 19 2008 westerba <antti.westerberg@fmi.fi> - 8.11.19-2.el5.fmi
- Compiled against new SmartMet API

* Thu Oct  9 2008 westerba <antti.westerberg@fmi.fi> - 8.10.9-1.el5.fmi
- Packaged operational and development files into separate packages

* Mon Sep 1 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.9.1-1.el5.fmi
- Initial build
