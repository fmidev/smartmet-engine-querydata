%define DIRNAME querydata
%define LIBNAME smartmet-%{DIRNAME}
%define SPECNAME smartmet-engine-%{DIRNAME}
Summary: SmartMet qengine engine
Name: %{SPECNAME}
Version: 25.6.17
Release: 1%{?dist}.fmi
License: MIT
Group: SmartMet/Engines
URL: https://github.com/fmidev/smartmet-engine-querydata
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

%define smartmet_fmt_min 11.0.0
%define smartmet_fmt_max 12.0.0

BuildRequires: %{smartmet_boost}-devel
BuildRequires: bzip2-devel
BuildRequires: gcc-c++
BuildRequires: gdal310-devel
BuildRequires: jsoncpp-devel >= 1.8.4
BuildRequires: make
BuildRequires: rpm-build
BuildRequires: zlib-devel
BuildRequires: fmt-devel >= %{smartmet_fmt_min}, fmt-devel < %{smartmet_fmt_max}
BuildRequires: smartmet-library-timeseries-devel >= 25.6.9
BuildRequires: smartmet-library-macgyver-devel >= 25.5.30
BuildRequires: smartmet-library-newbase-devel >= 25.3.20
BuildRequires: smartmet-library-spine-devel >= 25.5.13
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-serialization
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Requires: gdal310-libs
Requires: jsoncpp >= 1.8.4
Requires: fmt-libs >= %{smartmet_fmt_min}, fmt-libs < %{smartmet_fmt_max}
Requires: zlib
Requires: smartmet-library-timeseries >= 25.6.9
Requires: smartmet-library-macgyver >= 25.5.30
Requires: smartmet-library-newbase >= 25.3.20
Requires: smartmet-library-spine >= 25.5.13
#TestRequires: smartmet-utils-devel >= 25.2.18
#TestRequires: jsoncpp-devel >= 1.8.4
#TestRequires: gdal310-devel
#TestRequires: gcc-c++
Provides: %{LIBNAME}
Obsoletes: smartmet-brainstorm-qengine < 16.11.1
Obsoletes: smartmet-brainstorm-qengine-debuginfo < 16.11.1

%description
SmartMet querydata engine

%package -n %{SPECNAME}-devel
Summary: SmartMet %{SPECNAME} development headers
Group: SmartMet/Development
Provides: %{SPECNAME}-devel
Requires: gdal310-devel
Requires: %{SPECNAME} = %{version}-%{release}

Obsoletes: smartmet-brainstorm-qengine-devel < 16.11.1
%description -n %{SPECNAME}-devel
SmartMet %{SPECNAME} development headers.

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{SPECNAME}

%build -q -n %{SPECNAME}
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
* Tue Jun 17 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.6.17-1.fmi
- Improved support for point querydata parameters: wmo, lpnn, rwsid, distance, direction, stationtype

* Thu May 22 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.5.22-1.fmi
- Repackaged to hide dark() implementation details

* Tue Feb 18 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.2.18-1.fmi
- Update to gdal-3.10, geos-3.13 and proj-9.5

* Fri Jan 10 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.1.10-1.fmi
- Admin/info request update

* Sat Nov 30 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.11.30-1.fmi
- Added more support for querying metadata for pointwise querydata

* Wed Nov 13 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.11.13-1.fmi
- For speed use SpatialReference::hashValue for SR comparisons instead of exportToWkt

* Fri Nov  8 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.11.8-1.fmi
- Register admin requests to SmartMet::Spine::Reactor

* Tue Oct 15 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.15-1.fmi
- Removed support for landscaped parameters as obsolete

* Mon Sep  2 2024 Pertti Kinnia <pertti.kinnia@fmi.fi> 24.9.2-1.fmi
- Added list of validtimes to metadata (BRAINSTORM-3016)

* Wed Aug  7 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.8.7-1.fmi
- Update to gdal-3.8, geos-3.12, proj-94 and fmt-11

* Thu Aug  1 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.8.1-1.fmi
- Allow DEM and LandCover data to be missing when resampling data

* Tue Jul 30 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.30-1.fmi
- Model: update use of std::enable_shared_from_this

* Mon Jul 22 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.22-1.fmi
- Rebuild due to smartmet-library-macgyver ABI changes

* Fri Jul 12 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.12-1.fmi
- Replace many boost library types with C++ standard library ones

* Wed May 29 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.29-1.fmi
- Fix uninitialized shared ptr from previous release

* Tue May 28 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.28-1.fmi
- Remove uses of LocalTimePool

* Thu May 16 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.16-1.fmi
- Clean up boost date-time uses

* Mon May  6 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.6-1.fmi
- Use Date library (https://github.com/HowardHinnant/date) instead of boost date_time

* Fri Feb 23 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.23-1.fmi
- Full repackaging

* Wed Feb 21 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.2.21-1.fmi
- Repackaged due to TimeSeries ABI changes

* Fri Jan 19 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.1.19-1.fmi
- Fixed fmisid, stationlongitude and stationlatitude to work for locations which have a fmisid (BRAINSTORM-2840)

* Mon Dec  4 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.12.4-1.fmi
- Fix build after removing protobuf dependency

* Fri Dec  1 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.1-1.fmi
- Removed backend synchronization as obsolete (BRAINSTORM-2807)

* Thu Nov 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.16-1.fmi
- Handle climatology data without date 29.2.

* Thu Oct 12 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.10.12-1.fmi
- QImpl::levelValue: verify that level object is available

* Thu Aug 31 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.8.31-1.fmi
- Moved ParameterTranslations to spine

* Fri Jul 28 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.28-1.fmi
- Repackage due to bulk ABI changes in macgyver/newbase/spine

* Tue Jul 11 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.11-2.fmi
- Repackage due to changes master branch

* Tue Jul 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.11-1.fmi
- Silenced compiler warnings

* Thu Jun 15 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.15-1.fmi
- Fixed projection code to handle non-gridded querydata

* Tue Jun 13 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.13-1.fmi
- Support internal and environment variables in configuration files

* Tue Jun  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.6-1.fmi
- Return WGS84 envelope for point querydata too

* Thu Apr 27 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.4.27-1.fmi
- Repackage due to macgyver ABI changes (AsyncTask, AsyncTaskGroup)

* Tue Mar 21 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.3.21-1.fmi
- Allow disabling engine by not providing its configuration file

* Thu Mar  9 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.9-1.fmi
- Fixed destructors not to throw
- Silenced CodeChecker warnings

* Thu Dec 15 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.15-1.fmi
- Fixed thread safety issues discovered by helgrind

* Fri Dec  2 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.2-1.fmi
- Added a 'staticgrid' setting for models whose valid points do not change over time to speed up data loading

* Tue Nov 29 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.29-1.fmi
- Added safety check for calculating the hash value for querydata to protect against segmentation faults

* Fri Nov 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.25-1.fmi
- Improved error messages in case ValidPoints serialization fails

* Wed Nov 23 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.23-2.fmi
- Fixed a race condition by adding a mutex

* Wed Nov 23 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.23-1.fmi
- Optimized ValidPoints construction for speed

* Wed Oct  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.5-1.fmi
- Do not use boost::noncopyable

* Fri Sep  9 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.9-1.fmi
- Repackaged since TimeSeries library ABI changed

* Thu Sep  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.8-1.fmi
- Fixed sunrise etc to be formatted according to the set timeformat

* Thu Aug 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.25-1.fmi
- Use a generic exception handler for configuration file errors

* Wed Aug 24 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.24-1.fmi
- Fixed metadata resolution calculations

* Thu Aug 11 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.11-1.fmi
- SmartSymbolText translations are now read from the configuration file

* Mon Aug  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.8-1.fmi
- Enable aliases when searching for producers by name only (instead of with coordinates too)

* Thu Aug  4 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.4-1.fmi
- Added optional configuration settings for internal cache sizes

* Thu Jul 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.28-2.fmi
- If no valid time range is found from a single querydata, return full multifile instead to enable interpolation

* Thu Jul 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.28-1.fmi
- Added a method for fetching querydata for a valid time range

* Wed Jul 27 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.27-2.fmi
- Added missing cache statistics (Values and Coordinates)

* Wed Jul 27 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.27-1.fmi
- Repackaged since macgyver CacheStats ABI changed

* Wed Jul 20 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.7.20-1.fmi
- Repackage due to macgyver (AsynTaskGroup) ABI changes

* Fri Jun 17 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.6.17-1.fmi
- Add support for RHEL9. Update libpqxx to 7.7.0 (rhel8+) and fmt to 8.1.1

* Thu Jun  2 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.2-1.fmi
- Added NFmiArea::SetGridSize call to initialize fast access to LatLon coordinates

* Wed Jun  1 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.1-1.fmi
- Use SimpleWKT comparisons to see projections are equal despite +wktext etc

* Tue May 31 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.5.31-1.fmi
- Support engine disabling

* Tue May 24 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.24-1.fmi
- Repackaged due to NFmiArea ABI changes

* Fri May 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.20-2.fmi
- Small security improvements

* Fri May 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.20-1.fmi
- Repackaged due to ABI changes to newbase LatLon methods

* Thu May 19 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.19-1.fmi
- Removed obsolete WGS84 branch code

* Wed May  4 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.4-2.fmi
- GetValues now automatically converts kFloatMissing to NaN (cached results benefit tiled contouring)

* Fri Mar 18 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.3.18-1.fmi
- Update due to smartmet-library-spine and smartmet-library-timeseries changes

* Tue Mar 8 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.3.8-1.fmi
- Started using timeseries-library (BRAINSTORM-2259)

* Fri Jan 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.1.21-1.fmi
- Repackage due to upgrade of packages from PGDG repo: gdal-3.4, geos-3.10, proj-8.2

* Thu Jan 20 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.1.20-1.fmi
- Fixed WindUMS/WindVMS parameter pressure/height-query (BRAINSTORM-2236)

* Mon Jan 3 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.1.3-1.fmi
- Added support for new parameter max_latest_age (BRAINSTORM-2225)
- The new parameter tells the maximum age of the latest querydata file, so that
old files are not used if the latest file is missing

* Tue Dec  7 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.12.7-1.fmi
- Update to postgresql 13 and gdal 3.3

* Wed Nov 17 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.11.17-1.fmi
- Make it possible to clean validpoints directory at startup (BRAINSTORM-2186)
- clean_valid_points_cache_dir configuration parameter tells if directory is cleand at startup

* Tue Sep 28 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.9.28-1.fmi
- Repackage due to dependency change: moving libconfig files to differentr directory

* Mon Sep 13 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.9.13-1.fmi
- Repackaged due to Fmi::Cache statistics fixes

* Tue Sep  7 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.9.7-1.fmi
- Rebuild due to dependency changes

* Mon Aug 30 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.8.30-1.fmi
- Cache counters added (BRAINSTORM-1005)

* Sat Aug 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.21-1.fmi
- Repackaged due to LocalTimePool ABI changes

* Thu Aug 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.19-1.fmi
- Start using local time pool to avoid unnecessary allocations of local_date_time objects (BRAINSTORM-2122)

* Tue Aug 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.17-1.fmi
- Use new shutdown API

* Mon Aug  2 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.2-1.fmi
- Use atomic_shared_ptr instead of atomic_load/store

* Wed Jul 28 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.7.28-1.fmi
- Silenced compiler warnings

* Mon Jun 28 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.28-1.fmi
- Use newbase WGS84 define

* Thu Jun  3 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.3-1.fmi
- Cleaner shutdown

* Mon May 31 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.31-1.fmi
- Use only macgyver Hash.h

* Fri May 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.21-1.fmi
- Added QImpl::hashValue()

* Thu May 20 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.20-2.fmi
- Repackaged with improved hashing functions

* Thu May 20 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.20-1.fmi
- Use Fmi hash functions, boost::hash_combine produces too many collisions

* Thu May  6 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.6-1.fmi
- Repackaged due to ABI changes in NFmiAzimuthalArea

* Mon May  3 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.5.3-1.fmi
- Caching WGS84Envelopes (BRAINSTORM-1911)

* Thu Mar  4 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.4-1.fmi
- Restored LatLonCache, it is not needed only in the WGS84 branch

* Tue Mar  2 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.2-1.fmi
- Removed redundant to_lower_copy calls
- Reduce the number of local_date_time to NFmiMetTime conversions
- Prefer emplace_back when possible

* Mon Mar  1 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.1-1.fmi
- Improved WGS84Envelope locking

* Sat Feb 27 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.27-1.fmi
- Use FmiParameterName for metaparameters for speed

* Thu Feb 25 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.25-1.fmi
- Produce parameter info into a table in more suitable form for column sorting and searching

* Sat Feb 20 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.20-1.fmi
- Use OGR::gridNorth instead of querydata TrueNorthAzimuth

* Thu Feb 18 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.18-1.fmi
- Use NFmiArea::SpatialReference

* Mon Feb 15 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.15-1.fmi
- Added values() method needed by Download-plugin

* Wed Feb 10 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.10-1.fmi
- Use CoordinateMatrix APIs

* Mon Jan 25 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.1.25-1.fmi
- Report more info about producers, data, parameters (BRAINSTORM-1981)

* Thu Jan 14 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.14-1.fmi
- Repackaged smartmet to resolve debuginfo issues

* Wed Jan 13 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.13-1.fmi
- Repackaged with latest dependencies

* Wed Dec 30 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.12.30-1.fmi
- Rebuild due to jsoncpp upgrade for RHEL7

* Tue Dec 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.12.15-1.fmi
- Upgrade to pgdg12

* Tue Oct  6 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.6-1.fmi
- Enable sensible relative libconfig include paths

* Wed Sep 23 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.23-1.fmi
- Use Fmi::Exception instead of Spine::Exception

* Tue Sep 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.15-3.fmi
- Flush messages on querydata config changes immediately for faster log updates

* Tue Sep 15 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.9.15-2.fmi
- Add missing joining threads in RepoManager destructor

* Fri Sep  4 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.4-1.fmi
- Forward declare OGRSpatialReference in Engine.h to avoid dependency escalation

* Thu Sep  3 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.9.3-1.fmi
- Update engine shutdown support

* Fri Aug 28 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.28-1.fmi
- Reimplemented fetching full grids of calculated values such as WindChill for better speed and accuracy

* Thu Aug 27 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.27-1.fmi
- NFmiGrid API changed

* Wed Aug 26 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.26-1.fmi
- Repackaged due to NFmiGrid API changes

* Fri Aug 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.21-1.fmi
- Upgrade to fmt 6.2

* Mon Aug 17 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.17-1.fmi
- Repackaged due to OGRCoordinateTransformationFactory API changes

* Thu Aug 13 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.13-1.fmi
- GIS-library ABI changed

* Thu Jul  2 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.2-1.fmi
- SpatialReference API changed

* Wed May 13 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.5.13-1.fmi
- Repackaged since Spine Parameter class ABI changed

* Tue May  5 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.5.5-1.fmi
- Disable stack traces for user input errors

* Thu Apr 23 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.23-1.fmi
- Use newbase globe wrapping code for coordinates

* Wed Apr 22 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.22-1.fmi
- Improved gdal30/geos38 detection

* Mon Apr 20 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.20-1.fmi
- Added reading of parameter translations from the configuration file

* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.fmi
- Upgraded to Boost 1.69

* Tue Apr 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.14-1.fmi
- Use Fmi::CoordinateMatrix instead of NFmiCoordinateMatrix

* Wed Apr  8 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.8-1.fmi
- Added checking of projected metric coordinates for very elongated cells

* Mon Apr  6 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.6-1.fmi
- Using Fmi::CoordinateTransformation and Fmi::SpatialReference from now on

* Fri Apr  3 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.3-1.fmi
- Q::SpatialReference now returns a reference instead of a pointer

* Thu Apr  2 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.2-1.fmi
- Refactored API to use NFmiDataMatrix return values

* Mon Mar 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.3.30-1.fmi
- Use NFmiSpatialReference and NFmiCoordinateTransformation

* Thu Mar 19 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.3.19-1.fmi
- Added possibility to filter repo contents based on the producer name

* Thu Feb 13 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.13-1.fmi
- Forward declare GDAL classes in headers to avoid dependency escalation

* Fri Feb  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.7-1.fmi
- Repackaged due to newbase ABI changes

* Thu Jan 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.30-1.fmi
- hasProducer now recognizes aliases too

* Fri Dec 13 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.13-1.fmi
- Repackaged due to NFmiArea API changes

* Wed Dec 11 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.11-1.fmi
- Upgdare to GDAL 3.0 and GEOS 3.8

* Mon Nov 25 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.25-1.fmi
- Added Q::worldXY and Q location iteration methods

* Fri Nov 22 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.22-1.fmi
- Use std::vector instead of checkedVector

* Wed Nov 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.20-1.fmi
- Rebuilt due to newbase API changes

* Thu Oct 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.31-1.fmi
- Rebuilt due to newbase API/ABI changes

* Thu Sep 26 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.26-1.fmi
- Use atomic counters and status booleans (TSAN)
- Added ASAN & TSAN builds

* Wed Aug 28 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.8.28-1.fmi
- Repackaged since Spine::Location ABI changed

* Tue Aug  6 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.8.6-1.fmi
- Use std::async instead of boost::async for better exception propagation

* Mon Jul 29 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.7.29-1.fmi
- Improved error messages

* Thu Jun 27 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.27-1.fmi
- Define origintime for interpolated times too (first data with greater valid time)

* Thu Jun 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.20-1.fmi
- Return correct origintime for multifile data

* Wed Jun 19 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.19-1.fmi
- Fixed handling of missing values for WeatherSymbol calculations

* Thu Mar 21 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.3.21-1.fmi
- Allow producer list to be empty

* Fri Feb  8 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.8-1.fmi
- Use PrecipitationType if PotentialPrecipitationType is not available for SmartSymbol calculation

* Wed Feb  6 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.6-1.fmi
- Return only models with identical grids when multifile=true

* Mon Jan 28 2019 Pertti Kinnia <pertti.kinnia@fmi.fi> - 19.1.28-1.fmi
- Fixed weathernumber thunder probability classification (BS-1491)

* Tue Dec  4 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.12.4-1.fmi
- Repackaged since Spine::Table size changed

* Fri Nov 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.23-1.fmi
- Use PrecipitationType if PotentialPrecipitationType is not available in WeatherNumber calculation

* Fri Oct 19 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.10.19-1.fmi
- Added "mmap" setting for producers to be able to disable memory mapping

* Tue Sep 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.11-1.fmi
- Added calculation of 'weathernumber'

* Mon Aug 20 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.20-1.fmi
- Macgyver DirectoryMonitor callback API changed to use const references to avoid unnecessary copying
- Fixed several CodeChecker warnings

* Mon Aug 13 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.13-1.fmi
- Repackaged since Spine::Location size changed

* Mon Aug  6 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.6-1.fmi
- Silenced a large number of CodeChecker warnings

* Thu Aug  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.2-2.fmi
- Changed all umlaut-style character literals to be UTF-8 encoded to silence CodeChecker warnings

* Thu Aug  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.2-1.fmi
- Silenced several CodeChecker warnings

* Wed Jul 25 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.25-1.fmi
- Prefer nullptr over NULL

* Mon Jul 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.23-1.fmi
- Silenced CodeChecker warnings

* Thu Jun 28 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.28-1.fmi
- Fixed async executions to capture locals by reference to avoid locking memory mapped files

* Wed Jun 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.27-1.fmi
- More sensible destruction order for Model data members in order to avoid dangling pointers

* Thu Jun 21 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.21-1.fmi
- Report model removals in verbose mode

* Tue Jun 19 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.19-1.fmi
- Default number_to_keep is now 2

* Wed May 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.23-1.fmi
- Recompiled due to newbase ABI change

* Mon May 21 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.21-1.fmi
- Added regridding methods with relative_uv parameter to control whether U/V adjustment needs to be applied

* Tue May  8 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.8-1.fmi
- Added more special cases for queries of latlon coordinates

* Mon May  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.7-1.fmi
- Allow latitude queries to be both DataIndependent and DataDerived to keep WFS working

* Fri May  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.4-1.fmi
- Changed latitude and longitude to be data derived variables

* Thu May  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.3-1.fmi
- Report loaded querydata hash value in verbose mode

* Wed Apr 18 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.18-1.fmi
- Optimized repository metadata access calls for speed

* Fri Apr 13 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.13-1.fmi
- Increased max number of files per producer from 1,000 to 1,000,000 to be able to process large archives

* Tue Apr 10 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.10-1.fmi
- Added WindUMS and WindVMS handling for querydata which stores relative U/V components

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-1.fmi
- Upgrade to boost 1.66

* Fri Apr  6 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.6-1.fmi
- Include configured model properties in model hash values to regenerate products if necessary

* Tue Apr  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.3-1.fmi
- Report missing querydata paths during the initialization

* Tue Mar 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.27-2.fmi
- Added setting relative_uv. By default wind U/V components are not relative to the grid

* Tue Mar 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.27-1.fmi
- Added a separate thread for removing expired models

* Thu Mar 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.22-2.fmi
- Report deleted and expired querydata until multifile issues have been resolved

* Thu Mar 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.22-1.fmi
- Implemented GridNorth meta parameter

* Tue Mar 20 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.20-1.fmi
- Full repackaging of the server

* Sat Mar 17 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.17-2.fmi
- Added a max_age setting for data sources

* Sat Mar 17 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.17-1.fmi
- Added a Model constructor for data created on the fly as needed by WMS heatmaps

* Thu Mar  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.1-1.fmi
- Avoid locale copying in case conversions

* Tue Feb 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.27-2.fmi
- Fixed data independent value level to be height/pressure for respective queries

* Tue Feb 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.27-1.fmi
- Added a method to get meta parameters values in the original grid

* Thu Feb 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.22-1.fmi
- Symbol and SymbolText changed to SmartSymbol and SmartSymbolText

* Mon Feb 19 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.19-1.fmi
- Added setting valid_points_cache_dir with default "/var/smartmet/querydata/validpoints"

* Thu Feb 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.15-1.fmi
- Added parameter 'symbol', an improved weather symbol
- Added parameter 'symboltext'

* Wed Feb 14 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.14-1.fmi
- Beta version: added parameter 'symbol'

* Fri Feb  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.9-1.fmi
- Repackaged since base class SmartMetEngine size changed

* Thu Feb  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.1-1.fmi
- Added Engine::hasProducer to test whether the producer name is valid

* Mon Dec 11 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.11-1.fmi
- Return native world XY coodinates if spatial references match to avoid PROJ.4 inaccuracies

* Mon Dec  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.4-1.fmi
- Fixed repository manager to handle modified/overwritten files

* Tue Nov 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.28-1.fmi
- Added estimates for model expiration times

* Tue Oct 24 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.24-1.fmi
- Reduce verbose printing when using an older repo to reload querydata
- Detect changes to model settings during reload

* Mon Oct 23 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.23-2.fmi
- Made some configuration settings host specific

* Mon Oct 23 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.23-1.fmi
- Speed up reload by using the data in the current repository if possible

* Fri Oct 20 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.20-1.fmi
- Automatic reload of the configuration file and the respective data if the file changes

* Thu Oct 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.12-1.fmi
- Fixed engine initialization to finish even if all the directories are empty

* Wed Sep 20 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.20-1.fmi
- getWorldCoordinates now uses NaN instead of kFloatMissing

* Tue Sep 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.12-1.fmi
- Changed implementation of coordinate caching to prefer native latlon calculations due to PROJ.4 issues

* Sun Aug 27 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.28-1.fmi
- Upgrade to boost 1.65
- Use shared futures to cache coordinates and values to avoid duplicate work by simultaneous requests

* Wed May 24 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.24-1.fmi
- Avoid fetching DEM and LandCover variables if possible when sampling querydata

* Mon Apr 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.10-1.fmi
- Added Q::needsWraparound for identifying global data which needs an extra grid cell column to cover the entire globe

* Sat Apr  8 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.8-1.fmi
- Do not print stack traces for many trivial problems

* Tue Apr  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.4-1.fmi
- Support for pressure and height value queries

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-2.fmi
- Improved caching of querydata world coordinates

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-1.fmi
- Recompiled since Spine::Exception changed

* Tue Mar 14 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.14-1.fmi
- Switched to use macgyver StringConversion tools

* Sat Feb 11 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.11-1.fmi
- Repackaged due to newbase CreateNewArea API change

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
