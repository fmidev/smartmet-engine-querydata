// ======================================================================
/*!
 * \brief Implementation of QEngine
 */
// ======================================================================

#include "EngineImpl.h"
#include "MetaQueryFilters.h"
#include "RepoManager.h"
#include "Repository.h"
#include "Synchro.h"
#include "WGS84EnvelopeFactory.h"
#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <gis/SpatialReference.h>
#include <json/reader.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <spine/ConfigTools.h>
#include <spine/Convenience.h>
#include <spine/Exceptions.h>
#include <spine/Reactor.h>
#include <chrono>
#include <exception>
#include <iomanip>
#include <libconfig.h++>
#include <ogr_spatialref.h>
#include <system_error>

#define CHECK_LATEST_MODEL_AGE true

namespace
{
auto badcoord = std::make_pair(std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::quiet_NaN());
}

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{

// ----------------------------------------------------------------------
/*!
 * \brief The only permitted constructor requires a configfile
 */
// ----------------------------------------------------------------------

EngineImpl::EngineImpl(const std::string& configfile)
    : itsRepoManager(boost::make_shared<RepoManager>(configfile)),
      itsConfigFile(configfile),
      itsActiveThreadCount(0),
      itsParameterTranslations(boost::make_shared<Spine::ParameterTranslations>()),
      lastConfigErrno(EINPROGRESS)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Initializator (trivial in this case)
 */
// ----------------------------------------------------------------------
void EngineImpl::init()
{
  try
  {
    libconfig::Config config;

    // Enable sensible relative include paths
    boost::filesystem::path p = itsConfigFile;
    p.remove_filename();
    config.setIncludeDir(p.c_str());
    config.readFile(itsConfigFile.c_str());
    Spine::expandVariables(config);

    itsParameterTranslations = boost::make_shared<Spine::ParameterTranslations>(config);

    // Init caches
    int coordinate_cache_size = 100;
    int values_cache_size = 5000;
    config.lookupValue("cache.coordinates_size", coordinate_cache_size);
    config.lookupValue("cache.values_size", values_cache_size);

    itsCoordinateCache.resize(coordinate_cache_size);
    itsValuesCache.resize(values_cache_size);

    // Init querydata manager
    auto repomanager = itsRepoManager.load();
    repomanager->init();

    // Synchronize metadata
    itsSynchro = boost::make_shared<Synchronizer>(this, itsConfigFile);

    // Wait until all initial data has been loaded
    while (!repomanager->ready() && !Spine::Reactor::isShuttingDown())
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }

    // Then clean the old serialized validpoint files safely
    {
      Spine::ReadLock lock(repomanager->itsMutex);
      repomanager->cleanValidPointsCache();
    }

    // We got this far, assume config file must be valid
    lastConfigErrno = 0;

    // Start watcher thread to watch for configuration changes
    configFileWatcher = boost::thread(&EngineImpl::configFileWatch, this);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Watch the config file to change
 * Should be run in a separate thread
 */
// ----------------------------------------------------------------------
void EngineImpl::configFileWatch()
{
  boost::system::error_code ec;
  std::time_t filetime = getConfigModTime();

  while (!Spine::Reactor::isShuttingDown())
  {
    boost::this_thread::sleep_for(boost::chrono::seconds(1));

    // If file was deleted, skip and go waiting until it is back
    if (!boost::filesystem::exists(itsConfigFile, ec))
    {
      if (filetime > 0)
      {
        std::cout << "Querydata config " << itsConfigFile
                  << " removed - current configuration kept until new file appears" << std::endl;
        filetime = 0;
        lastConfigErrno = ENOENT;
      }
      continue;
    }

    std::time_t newfiletime = boost::filesystem::last_write_time(itsConfigFile, ec);

    // Was the file modified?
    if (newfiletime != filetime && !Spine::Reactor::isShuttingDown())
    {
      // File changed
      // Go into cooling period of waiting a few seconds and checking again
      // This assures there are no half completed writes
      lastConfigErrno = EINPROGRESS;

      try
      {
        while (newfiletime != filetime && !Spine::Reactor::isShuttingDown())
        {
          std::cout << Spine::log_time_str() + " Querydata config " + itsConfigFile +
                           " updated, rereading"
                    << std::endl;
          filetime = newfiletime;
          boost::this_thread::sleep_for(boost::chrono::seconds(3));
          newfiletime = boost::filesystem::last_write_time(itsConfigFile, ec);
        }

        if (!Spine::Reactor::isShuttingDown())
        {
          // Generate new repomanager according to new configs
          boost::shared_ptr<RepoManager> newrepomanager =
              boost::make_shared<RepoManager>(itsConfigFile);

          // The old manager can be used to initialize common data faster
          auto oldrepomanager = itsRepoManager.load();
          newrepomanager->setOldManager(oldrepomanager);
          newrepomanager->init();

          // Wait until all initial data has been loaded
          while (!newrepomanager->ready() && !Spine::Reactor::isShuttingDown())
          {
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
          }

          newrepomanager->removeOldManager();

          if (!Spine::Reactor::isShuttingDown())
          {
            // Update current repomanager
            itsRepoManager.store(newrepomanager);
            std::cout << Spine::log_time_str() + " Querydata config " + itsConfigFile +
                             " update done"
                      << std::endl;
            lastConfigErrno = 0;
            // Before poll cycling again, wait to avoid constant reload if the file changes many
            // times
            boost::this_thread::sleep_for(boost::chrono::seconds(2));
          }
        }
      }
      catch (const std::exception& e)
      {
        if (strstr(e.what(), "syntax error") != nullptr)
          lastConfigErrno = ENOEXEC;
        std::cerr << std::string{"Error reading new config: "} + e.what() + "\n";
      }

      filetime = newfiletime;  // Update time even if there is an error
      // We don't want to reread a damaged file continuously
    }
  }

  // Exit on shutdown
  lastConfigErrno = ESHUTDOWN;
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown the engine
 */
// ----------------------------------------------------------------------

void EngineImpl::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (qengine)\n";

    if (configFileWatcher.joinable())
    {
      configFileWatcher.interrupt();
      configFileWatcher.join();
    }

    auto repomanager = itsRepoManager.load();

    if (repomanager != nullptr)
      repomanager->shutdown();

    if (itsSynchro)
      itsSynchro->shutdown();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get caches sizes
 */
// ----------------------------------------------------------------------

CacheReportingStruct EngineImpl::getCacheSizes() const
{
  return CacheReportingStruct{itsCoordinateCache.maxSize(),
                              itsCoordinateCache.size(),
                              itsValuesCache.maxSize(),
                              itsValuesCache.size()};
}

// ----------------------------------------------------------------------
/*!
 * \brief Get list of available producers
 */
// ----------------------------------------------------------------------

const ProducerList& EngineImpl::producers() const
{
  try
  {
    // We didn't bother to add a producers() method for the Impl,
    // hence we take an explicit lock here instead of inside the Impl.
    // The other public methods call the Impl to do the job.
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsProducerList;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given producer is defined
 */
// ----------------------------------------------------------------------

bool EngineImpl::hasProducer(const Producer& producer) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.hasProducer(producer);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get list of available origintimes for a producer
 */
// ----------------------------------------------------------------------

OriginTimes EngineImpl::origintimes(const Producer& producer) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.originTimes(producer);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data for the given producer
 */
// ----------------------------------------------------------------------

Q EngineImpl::get(const Producer& producer) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);
    auto q = repomanager->itsRepo.get(producer);
    q->setParameterTranslations(itsParameterTranslations.load());
    return q;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data for the given producer and origintime
 */
// ----------------------------------------------------------------------

Q EngineImpl::get(const Producer& producer, const Fmi::DateTime& origintime) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);
    auto q = repomanager->itsRepo.get(producer, origintime);
    q->setParameterTranslations(itsParameterTranslations.load());
    return q;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data for the given producer and valid time period
 */
// ----------------------------------------------------------------------

Q EngineImpl::get(const Producer& producer, const boost::posix_time::time_period& timePeriod) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);
    auto q = repomanager->itsRepo.get(producer, timePeriod);
    q->setParameterTranslations(itsParameterTranslations.load());
    return q;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select first model which covers the given point
 *
 * Returns empty producer if there are no matches.
 */
// ----------------------------------------------------------------------

Producer EngineImpl::find(double lon,
                          double lat,
                          double maxdist,
                          bool usedatamaxdistance,
                          const std::string& leveltype) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.find(repomanager->itsProducerList,
                                     repomanager->itsProducerList,
                                     lon,
                                     lat,
                                     maxdist,
                                     usedatamaxdistance,
                                     leveltype,
                                     CHECK_LATEST_MODEL_AGE);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select first allowed model which covers the given point
 *
 * Returns empty producer if there are no matches.
 */
// ----------------------------------------------------------------------

Producer EngineImpl::find(const ProducerList& producerlist,
                          double longitude,
                          double latitude,
                          double maxdistance,
                          bool usedatamaxdistance,
                          const std::string& leveltype) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.find(producerlist,
                                     repomanager->itsProducerList,
                                     longitude,
                                     latitude,
                                     maxdistance,
                                     usedatamaxdistance,
                                     leveltype);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Return info of producers as table
 */
// ----------------------------------------------------------------------

Repository::ContentTable EngineImpl::getProducerInfo(
    const std::string& timeFormat, const boost::optional<std::string>& producer) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);

    ProducerList producerList;
    if (producer)
      producerList.push_back(*producer);

    return repomanager->itsRepo.getProducerInfo(
        producer ? producerList : repomanager->itsProducerList, timeFormat);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Return info of parameters as table
 */
// ----------------------------------------------------------------------

Repository::ContentTable EngineImpl::getParameterInfo(
    const boost::optional<std::string>& producer) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);

    ProducerList producerList;
    if (producer)
      producerList.push_back(*producer);

    return repomanager->itsRepo.getParameterInfo(producer ? producerList
                                                          : repomanager->itsProducerList);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Return currently mapped files as table
 */
// ----------------------------------------------------------------------

Repository::ContentTable EngineImpl::getEngineContentsForAllProducers(
    const std::string& timeFormat, const std::string& projectionFormat) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);

    return repomanager->itsRepo.getRepoContents(timeFormat, projectionFormat);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Return currently mapped files for a producer as table
 */
// ----------------------------------------------------------------------

Repository::ContentTable EngineImpl::getEngineContentsForProducer(
    const std::string& producer,
    const std::string& timeFormat,
    const std::string& projectionFormat) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);

    if (producer.empty())
      return repomanager->itsRepo.getRepoContents(timeFormat, projectionFormat);
    return repomanager->itsRepo.getRepoContents(producer, timeFormat, projectionFormat);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the time period for the given producer
 *
 * Returns a timeperiod for which is_null() is true if there is no data.
 */
// ----------------------------------------------------------------------

boost::posix_time::time_period EngineImpl::getProducerTimePeriod(const Producer& producer) const
{
  try
  {
    // Handle unknown producers such as observations quickly without exceptions
    if (!hasProducer(producer))
    {
      return {Fmi::DateTime(), Fmi::Hours(0)};  // is_null will return true
    }

    try
    {
      auto q = get(producer);
      auto validtimes = q->validTimes();
      if (validtimes->empty())
        return {Fmi::DateTime(),
                Fmi::Hours(0)};  // is_null will return true

      return {validtimes->front(), validtimes->back()};
    }
    catch (...)
    {
      return {Fmi::DateTime(), Fmi::Hours(0)};  // is_null will return true
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> EngineImpl::getEngineMetadataBasic() const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);

    return repomanager->itsRepo.getRepoMetadata();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> EngineImpl::getEngineMetadataWithOptions(
    const MetaQueryOptions& theOptions) const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);

    return repomanager->itsRepo.getRepoMetadata(theOptions);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> EngineImpl::getEngineSyncMetadataBasic(const std::string& syncGroup) const
{
  try
  {
    auto syncProducers = itsSynchro->getSynchedData(syncGroup);

    if (!syncProducers)
      return {};  // Unknown sync group

    auto repomanager = itsRepoManager.load();

    std::list<MetaData> repocontent;
    {
      Spine::ReadLock lock(repomanager->itsMutex);
      repocontent = repomanager->itsRepo.getRepoMetadata();
    }

    if (repocontent.empty())
      return repocontent;  // No point filtering an empty list

    for (auto iter = repocontent.begin(); iter != repocontent.end();)
    {
      auto& producer = iter->producer;

      auto syncIt = syncProducers->find(producer);
      if (syncIt == syncProducers->end())
      {
        // This producer is not available in this synchronization group
        repocontent.erase(iter++);
        continue;
      }

      // Filter according to synchroed origin times
      if (!filterSynchro(*iter, syncIt->second))
      {
        repocontent.erase(iter++);
        continue;
      }

      ++iter;
    }

    return repocontent;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> EngineImpl::getEngineSyncMetadataWithOptions(
    const std::string& syncGroup, const MetaQueryOptions& options) const
{
  try
  {
    auto syncProducers = itsSynchro->getSynchedData(syncGroup);
    auto repomanager = itsRepoManager.load();

    if (!syncProducers)
      return {};  // Unknown sync group

    std::list<MetaData> repocontent;
    {
      Spine::ReadLock lock(repomanager->itsMutex);
      repocontent = repomanager->itsRepo.getRepoMetadata(options);
    }

    if (repocontent.empty())
      return repocontent;  // No point filtering an empty list

    for (auto iter = repocontent.begin(); iter != repocontent.end();)
    {
      auto& producer = iter->producer;
      auto syncIt = syncProducers->find(producer);
      if (syncIt == syncProducers->end())
      {
        // This producer is not available in this synchronization group
        repocontent.erase(iter++);
        continue;
      }

      // Filter according to synchroed origin times
      if (!filterSynchro(*iter, syncIt->second))
      {
        repocontent.erase(iter++);
        continue;
      }

      ++iter;
    }

    return repocontent;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Repository::MetaObject EngineImpl::getSynchroInfos() const
{
  try
  {
    auto repomanager = itsRepoManager.load();

    Spine::ReadLock lock(repomanager->itsMutex);

    return repomanager->itsRepo.getSynchroInfos();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::optional<ProducerMap> EngineImpl::getSyncProducers(const std::string& syncGroup) const
{
  try
  {
    return itsSynchro->getSynchedData(syncGroup);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EngineImpl::startSynchronize(Spine::Reactor* theReactor)
{
  try
  {
    itsSynchro->launch(theReactor);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Get producer's configuration
 */
// ----------------------------------------------------------------------

const ProducerConfig& EngineImpl::getProducerConfig(const std::string& producer) const
{
  try
  {
    auto repomanager = itsRepoManager.load();
    return repomanager->producerConfig(producer);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Mark the given coordinate cell as bad
 */
// ----------------------------------------------------------------------

void mark_cell_bad(Fmi::CoordinateMatrix& theCoords, const NFmiPoint& theCoord)
{
  try
  {
    if (theCoord.X() == kFloatMissing || theCoord.Y() == kFloatMissing ||
        std::isnan(theCoord.X()) || std::isnan(theCoord.Y()))

      return;

    if (theCoord.X() >= 0 && theCoord.X() < theCoords.width() - 1 && theCoord.Y() >= 0 &&
        theCoord.Y() < theCoords.height() - 1)
    {
      auto i = static_cast<std::size_t>(theCoord.X());
      auto j = static_cast<std::size_t>(theCoord.Y());
      theCoords(i + 0, j + 0) = badcoord;
      theCoords(i + 1, j + 0) = badcoord;
      // Marking two vertices bad is enough to invalidate the cell
      // theCoords(i+0,j+1])= badcoord;
      // theCoords(i+1],+1])= badcoord;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Project coordinates
 */
// ----------------------------------------------------------------------

CoordinatesPtr project_coordinates(const CoordinatesPtr& theCoords,
                                   const Q& theQ,
                                   const Fmi::SpatialReference& theSR)
{
  try
  {
    // Copy the original coordinates for projection

    Fmi::CoordinateTransformation transformation(theQ->SpatialReference(), theSR);
    auto coords = std::make_shared<Fmi::CoordinateMatrix>(*theCoords);

    coords->transform(transformation);

    // If the target SR is geographic, we must discard the grid cells containing
    // the north or south poles since the cell vertex coordinates wrap around
    // the world. The more difficult alternative would be to divide the cell into
    // 4 triangles from the poles and contour the triangles.

    // We also have to check whether some grid cells cross the 180th meridian
    // and discard them

    // If the target SR is no geographic, we discard all very elongated cells
    // since they are likely spanning the world

    // TODO: Should probably be removed in favour of the grid analyzer

#if 0    
    if (theSR.isGeographic() != 0)
    {
      auto& c = *coords;

      const auto& grid = theQ->grid();
      auto northpole = grid.LatLonToGrid(0, 90);
      mark_cell_bad(c, northpole);
      auto southpole = grid.LatLonToGrid(0, -90);
      mark_cell_bad(c, southpole);

      const auto nx = c.width();
      const auto ny = c.height();

      for (std::size_t j = 0; j < ny; j++)
        for (std::size_t i = 0; i + 1 < nx; i++)
        {
          double lon1 = c.x(i, j);
          double lon2 = c.x(i + 1, j);
          if (lon1 != kFloatMissing && lon2 != kFloatMissing && std::abs(lon1 - lon2) > 180)
            c.set(i, j, badcoord);
        }
    }
#endif

    return coords;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

CoordinatesPtr EngineImpl::getWorldCoordinatesForSR(const Q& theQ,
                                                    const Fmi::SpatialReference& theSR) const
{
  try
  {
    // Hash value of original WorldXY coordinates
    auto qhash = theQ->gridHashValue();

    // Hash value of projected coordinates
    auto projhash = qhash;

    // Return original world XY directly with get_world_xy if spatial
    // references match This is absolutely necessary to avoid gaps in
    // WMS tiles since with proj(invproj(p)) may differ significantly
    // from p outside the valid area of the projection.

    const auto& dataSR = theQ->info()->SpatialReference();
    auto datawkt = Fmi::OGR::exportToSimpleWkt(dataSR);

    auto reqwkt = Fmi::OGR::exportToSimpleWkt(theSR);

    if (datawkt != reqwkt)
      Fmi::hash_combine(projhash, theSR.hashValue());

    if (qhash == projhash)
      return getWorldCoordinates(theQ);

    // Search cache for the projected coordinates
    auto cached_coords = itsCoordinateCache.find(projhash);
    if (cached_coords)
      return cached_coords->get();

    // Now we need to to get WorldXY coordinates - this is fast
    auto worldxy = getWorldCoordinates(theQ);

    // Project to target SR. Do NOT use intermediate latlons in any datum, or the Z value will not
    // be included in all stages of the projection, and large errors will occur if the datums
    // differ significantly (e.g. sphere vs ellipsoid)

    auto ftr2 = std::async([&] { return project_coordinates(worldxy, theQ, theSR); }).share();

    itsCoordinateCache.insert(projhash, ftr2);
    return ftr2.get();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

CoordinatesPtr EngineImpl::getWorldCoordinatesDefault(const Q& theQ) const
{
  return std::make_shared<Fmi::CoordinateMatrix>(theQ->FullCoordinateMatrix());
}

// ----------------------------------------------------------------------
/*!
 * \brief Change all kFloatMissing values to NaN
 */
// ----------------------------------------------------------------------

void set_missing_to_nan(NFmiDataMatrix<float>& values)
{
  const std::size_t nx = values.NX();
  const std::size_t ny = values.NY();
  if (nx == 0 || ny == 0)
    return;

  const auto nan = std::numeric_limits<float>::quiet_NaN();

  // Unfortunately NFmiDataMatrix is a vector of vectors, memory
  // access patterns are not optimal

  for (std::size_t i = 0; i < nx; i++)
  {
    auto& tmp = values[i];
    for (std::size_t j = 0; j < ny; j++)
      if (tmp[j] == kFloatMissing)
        tmp[j] = nan;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data values and change kFloatMissing to NaN
 */
// ----------------------------------------------------------------------

ValuesPtr get_values(const Q& theQ, Fmi::DateTime theTime)
{
  auto ret = std::make_shared<Values>(theQ->values(theTime));
  set_missing_to_nan(*ret);
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the data values
 *
 * Retrieval is done asynchronously through a shared future so that for
 * example multiple WMS tile requests would not cause the same values
 * to be retrieved twice.
 */
// ----------------------------------------------------------------------

ValuesPtr EngineImpl::getValuesDefault(const Q& theQ,
                                       std::size_t theValuesHash,
                                       Fmi::DateTime theTime) const
{
  try
  {
    // If there is a future in the cache, ask it for the values

    auto values = itsValuesCache.find(theValuesHash);
    if (values)
      return values->get();

    // Else create a shared future for calculating the values
    auto ftr = std::async(std::launch::async, [&] { return get_values(theQ, theTime); }).share();

    // Store the shared future into the cache for other threads to see too
    itsValuesCache.insert(theValuesHash, ftr);

    // And wait for the future to finish along with other threads
    return ftr.get();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to retrieve data")
        .addParameter("time", Fmi::to_iso_extended_string(theTime));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data values and change kFloatMissing to NaN
 */
// ----------------------------------------------------------------------

ValuesPtr get_values(const Q& theQ,
                     const Spine::Parameter& theParam,
                     Fmi::DateTime theTime)
{
  auto ret = std::make_shared<Values>(theQ->values(theParam, theTime));
  set_missing_to_nan(*ret);
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the data values
 *
 * Retrieval is done asynchronously through a shared future so that for
 * example multiple WMS tile requests would not cause the same values
 * to be retrieved twice.
 */
// ----------------------------------------------------------------------

ValuesPtr EngineImpl::getValuesForParam(const Q& theQ,
                                        const Spine::Parameter& theParam,
                                        std::size_t theValuesHash,
                                        Fmi::DateTime theTime) const
{
  try
  {
    // If there is a future in the cache, ask it for the values

    auto values = itsValuesCache.find(theValuesHash);
    if (values)
      return values->get();

    // Else create a shared future for calculating the values
    auto ftr =
        std::async(std::launch::async, [&] { return get_values(theQ, theParam, theTime); }).share();

    // Store the shared future into the cache for other threads to see too
    itsValuesCache.insert(theValuesHash, ftr);

    // And wait for the future to finish along with other threads
    return ftr.get();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to retrieve data")
        .addParameter("time", Fmi::to_iso_extended_string(theTime));
  }
}

std::time_t EngineImpl::getConfigModTime()
{
  auto repomanager = itsRepoManager.load();
  return repomanager->getConfigModTime();
}

int EngineImpl::getLastConfigErrno()
{
  return lastConfigErrno;
}

Fmi::Cache::CacheStatistics EngineImpl::getCacheStats() const
{
  Fmi::Cache::CacheStatistics ret;

  auto repomanager = itsRepoManager.load();
  ret["Querydata::lat_lon_cache"] = repomanager->getCacheStats();
  ret["Querydata::wgs84_envelope_cache"] = WGS84EnvelopeFactory::getCacheStats();
  ret["Querydata::values_cache"] = itsValuesCache.statistics();
  ret["Querydata::coordinate_cache"] = itsCoordinateCache.statistics();
  return ret;
}

Engine* EngineImpl::create(const std::string& configfile)
{
  try
  {
    const bool disabled = [&configfile]()
    {
      const char* name = "SmartMet::Engine::QueryData::EngineImpl::create";
      if (configfile.empty())
      {
        std::cout << Spine::log_time_str() << ' ' << ANSI_FG_RED << name
                  << ": configuration file not specified or its name is empty string: "
                  << "engine disabled." << ANSI_FG_DEFAULT << std::endl;
        return true;
      }

      SmartMet::Spine::ConfigBase cfg(configfile);
      const bool result = cfg.get_optional_config_param<bool>("disabled", false);
      if (result)
        std::cout << Spine::log_time_str() << ' ' << ANSI_FG_RED << name << ": engine disabled"
                  << ANSI_FG_DEFAULT << std::endl;
      return result;
    }();

    if (disabled)
      return new Engine();
    return new EngineImpl(configfile);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// DYNAMIC MODULE CREATION TOOLS

extern "C" void* engine_class_creator(const char* configfile, void* /* user_data */)
{
  return SmartMet::Engine::Querydata::EngineImpl::create(configfile);
}

extern "C" const char* engine_name()
{
  return "Querydata";
}
// ======================================================================
