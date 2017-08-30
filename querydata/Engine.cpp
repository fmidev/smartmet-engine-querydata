// ======================================================================
/*!
 * \brief Implementation of QEngine
 */
// ======================================================================

#include "Engine.h"
#include "MetaQueryFilters.h"
#include "RepoManager.h"
#include "Repository.h"
#include "Synchro.h"
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <chrono>
#include <system_error>

#include <spine/Exception.h>
#include <exception>

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

Engine::Engine(const std::string& configfile)
    : itsSynchro(), itsConfigFile(configfile), itsActiveThreadCount(0), lastConfigErrno(EINPROGRESS)
{
  auto repomanager = boost::make_shared<RepoManager>(configfile);
  boost::atomic_store(&itsRepoManager, repomanager);
}

// ----------------------------------------------------------------------
/*!
 * \brief Initializator (trivial in this case)
 */
// ----------------------------------------------------------------------
void Engine::init()
{
  try
  {
    itsCoordinateCache.resize(100);
    itsValuesCache.resize(5000);
    auto repomanager = boost::atomic_load(&itsRepoManager);
    repomanager->init();
    itsSynchro.reset(new Synchronizer(this, itsConfigFile));

    // Wait until all initial data has been loaded
    while (!repomanager->ready() && !itsShutdownRequested)
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }

    // We got this far, assume config file must be valid
    lastConfigErrno = 0;

    // Start watcher thread to watch for configuration changes
    configFileWatcher = boost::thread(&Engine::configFileWatch, this);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Watch the config file to change
 * Should be run in a separate thread
 */
// ----------------------------------------------------------------------
void Engine::configFileWatch()
{
  boost::system::error_code ec;
  std::time_t filetime = getConfigModTime();

  while (1 == 1)
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
    if (newfiletime != filetime)
    {
      // File changed
      // Go into cooling period of waiting a few seconds and checking again
      // This assures there are no half completed writes
      lastConfigErrno = EINPROGRESS;

      try
      {
        while (newfiletime != filetime)
        {
          std::cout << (std::string) "Querydata config " + itsConfigFile + " updated, rereading\n";
          filetime = newfiletime;
          boost::this_thread::sleep_for(boost::chrono::seconds(3));
          newfiletime = boost::filesystem::last_write_time(itsConfigFile, ec);
        }

        // Generate new repomanager according to new configs
        boost::shared_ptr<RepoManager> newrepomanager =
            boost::make_shared<RepoManager>(itsConfigFile);
        newrepomanager->init();
        // Wait until all initial data has been loaded
        while (!newrepomanager->ready() && !itsShutdownRequested)
        {
          boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }

        // Update current repomanager
        boost::atomic_store(&itsRepoManager, newrepomanager);
        std::cout << (std::string) "Querydata config " + itsConfigFile + " update done\n";
        lastConfigErrno = 0;
        // Before poll cycling again, wait to avoid constant reload if the file changes many times
        boost::this_thread::sleep_for(boost::chrono::seconds(2));
      }
      catch (std::exception& e)
      {
        if (strstr(e.what(), "syntax error") != nullptr)
          lastConfigErrno = ENOEXEC;
        std::cerr << (std::string) "Error reading new config: " + e.what() + "\n";
      }

      filetime = newfiletime;  // Update time even if there is an error
      // We don't want to reread a damaged file continuously
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown the engine
 */
// ----------------------------------------------------------------------

void Engine::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (qengine)\n";
    auto repomanager = boost::atomic_load(&itsRepoManager);

    if (repomanager != NULL)
      repomanager->shutdown();

    if (itsSynchro)
      itsSynchro->shutdown();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::shutdownRequestFlagSet()
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    if (repomanager != NULL)
      repomanager->shutdownRequestFlagSet();

    if (itsSynchro)
      itsSynchro->shutdownRequestFlagSet();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get caches sizes
 */
// ----------------------------------------------------------------------

CacheReportingStruct Engine::getCacheSizes() const
{
  try
  {
    CacheReportingStruct ret;

    ret.coordinate_cache_max_size = itsCoordinateCache.maxSize();
    ret.coordinate_cache_size = itsCoordinateCache.size();
    ret.values_cache_max_size = itsValuesCache.maxSize();
    ret.values_cache_size = itsValuesCache.size();

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get list of available producers
 */
// ----------------------------------------------------------------------

const ProducerList& Engine::producers() const
{
  try
  {
    // We didn't bother to add a producers() method for the Impl,
    // hence we take an explicit lock here instead of inside the Impl.
    // The other public methods call the Impl to do the job.
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsProducerList;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get list of available origintimes for a producer
 */
// ----------------------------------------------------------------------

OriginTimes Engine::origintimes(const Producer& producer) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.originTimes(producer);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data for the given producer
 */
// ----------------------------------------------------------------------

Q Engine::get(const Producer& producer) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.get(producer);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data for the given producer and origintime
 */
// ----------------------------------------------------------------------

Q Engine::get(const Producer& producer, const boost::posix_time::ptime& origintime) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.get(producer, origintime);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select first model which covers the given point
 *
 * Returns empty producer if there are no matches.
 */
// ----------------------------------------------------------------------

Producer Engine::find(double lon,
                      double lat,
                      double maxdist,
                      bool usedatamaxdistance,
                      const std::string& leveltype) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.find(repomanager->itsProducerList,
                                     repomanager->itsProducerList,
                                     lon,
                                     lat,
                                     maxdist,
                                     usedatamaxdistance,
                                     leveltype);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select first allowed model which covers the given point
 *
 * Returns empty producer if there are no matches.
 */
// ----------------------------------------------------------------------

Producer Engine::find(const ProducerList& producerlist,
                      double longitude,
                      double latitude,
                      double maxdistance,
                      bool usedatamaxdistance,
                      const std::string& leveltype) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Return available valid times for the latest model
 */
// ----------------------------------------------------------------------

#if 0
	std::list<boost::posix_time::ptime>
	Engine::validtimes(const Producer & producer) const
	{
	    auto repomanager=boost::atomic_load(&itsRepoManager);

	  Spine::ReadLock lock(repomanager->itsMutex);
	  return repomanager->validtimes(producer);
	}
#endif

// ----------------------------------------------------------------------
/*!
 *\ brief Return currently mapped files as table
 */
// ----------------------------------------------------------------------

Repository::ContentTable Engine::getEngineContents(const std::string& timeFormat,
                                                   const std::string& projectionFormat) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);

    return repomanager->itsRepo.getRepoContents(timeFormat, projectionFormat);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the time period for the given producer
 *
 * Returns a timeperiod for which is_null() is true if there is no data.
 */
// ----------------------------------------------------------------------

boost::posix_time::time_period Engine::getProducerTimePeriod(const Producer& producer) const
{
  try
  {
    try
    {
      auto q = get(producer);
      auto validtimes = q->validTimes();
      if (validtimes->empty())
        return boost::posix_time::time_period(
            boost::posix_time::ptime(), boost::posix_time::hours(0));  // is_null will return true
      else
        return boost::posix_time::time_period(validtimes->front(), validtimes->back());
    }
    catch (...)
    {
      return boost::posix_time::time_period(
          boost::posix_time::ptime(), boost::posix_time::hours(0));  // is_null will return true
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::list<MetaData> Engine::getEngineMetadata() const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);

    return repomanager->itsRepo.getRepoMetadata();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::list<MetaData> Engine::getEngineMetadata(const MetaQueryOptions& theOptions) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);

    return repomanager->itsRepo.getRepoMetadata(theOptions);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::list<MetaData> Engine::getEngineSyncMetadata(const std::string& syncGroup) const
{
  try
  {
    auto syncProducers = itsSynchro->getSynchedData(syncGroup);

    if (syncProducers)
    {
      auto repomanager = boost::atomic_load(&itsRepoManager);

      std::list<MetaData> repocontent;
      {
        Spine::ReadLock lock(repomanager->itsMutex);

        repocontent = repomanager->itsRepo.getRepoMetadata();
      }

      if (repocontent.empty())
      {
        // No point filtering an empty list
        return repocontent;
      }

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
    else
    {
      // Unknown sync group
      return std::list<MetaData>();
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::list<MetaData> Engine::getEngineSyncMetadata(const std::string& syncGroup,
                                                  const MetaQueryOptions& options) const
{
  try
  {
    auto syncProducers = itsSynchro->getSynchedData(syncGroup);
    auto repomanager = boost::atomic_load(&itsRepoManager);

    if (syncProducers)
    {
      std::list<MetaData> repocontent;
      {
        Spine::ReadLock lock(repomanager->itsMutex);

        repocontent = repomanager->itsRepo.getRepoMetadata(options);
      }

      if (repocontent.empty())
      {
        // No point filtering an empty list
        return repocontent;
      }

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
    else
    {
      // Unknown sync group
      return std::list<MetaData>();
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

Repository::MetaObject Engine::getSynchroInfos() const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);

    return repomanager->itsRepo.getSynchroInfos();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

boost::optional<ProducerMap> Engine::getSyncProducers(const std::string& syncGroup) const
{
  try
  {
    return itsSynchro->getSynchedData(syncGroup);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Engine::startSynchronize(Spine::Reactor* theReactor)
{
  try
  {
    itsSynchro->launch(theReactor);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Get data for given producer and optional origintime inside given
 *\		  validtime range
 */
// ----------------------------------------------------------------------

#if 0
	SharedModelTimeList Engine::get(const Producer & producer,
									const boost::posix_time::ptime & starttime,
									const boost::posix_time::ptime & endtime,
									unsigned int timestep,
									const OriginTime & origintime,
									bool & timeinterpolation) const
	{
	    auto repomanager=boost::atomic_load(&itsRepoManager);

	  Spine::ReadLock lock(repomanager->itsMutex);
	  return repomanager->itsRepo.get(producer,starttime,endtime,timestep,origintime,timeinterpolation);
	}
#endif

// ----------------------------------------------------------------------
/*!
 *\ brief Get producer's configuration
 */
// ----------------------------------------------------------------------

const ProducerConfig& Engine::getProducerConfig(const std::string& producer) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);
    return repomanager->producerConfig(producer);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::size_t hash_value(const OGRSpatialReference& theSR)
{
  try
  {
    char* wkt;
    theSR.exportToWkt(&wkt);
    std::string tmp(wkt);
    OGRFree(wkt);
    boost::hash<std::string> hasher;
    return hasher(tmp);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

NFmiDataMatrix<NFmiPoint> get_world_xy(const Q& theQ)
{
  try
  {
    if (!theQ->isGrid())
      throw Spine::Exception(BCP, "Trying to contour non-gridded data");

    // For latlon data GridToWorldXY returns metric units even though we want geographic coordinates
    auto id = theQ->area().ClassId();
    bool islatlon = (id == kNFmiLatLonArea || id == kNFmiRotatedLatLonArea);

    const auto& grid = theQ->grid();

    const long unsigned int nx = grid.XNumber();
    const long unsigned int ny = grid.YNumber();

    NFmiDataMatrix<NFmiPoint> coords(nx, ny);

    for (std::size_t j = 0; j < ny; j++)
      for (std::size_t i = 0; i < nx; i++)
        if (islatlon)
          coords[i][j] = grid.GridToLatLon(i, j);
        else
          coords[i][j] = grid.GridToWorldXY(i, j);

    return coords;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Mark the given coordinate cell as bad
 */
// ----------------------------------------------------------------------

void mark_cell_bad(Coordinates& theCoords, const NFmiPoint& theCoord)
{
  try
  {
    if (theCoord.X() == kFloatMissing || theCoord.Y() == kFloatMissing)
      return;

    if (theCoord.X() >= 0 && theCoord.X() < theCoords.NX() - 1 && theCoord.Y() >= 0 &&
        theCoord.Y() < theCoords.NY() - 1)
    {
      std::size_t i = static_cast<std::size_t>(theCoord.X());
      std::size_t j = static_cast<std::size_t>(theCoord.Y());
      NFmiPoint badcoord(kFloatMissing, kFloatMissing);
      theCoords[i + 0][j + 0] = badcoord;
      theCoords[i + 1][j + 0] = badcoord;
      // Marking two vertices bad is enough to invalidate the cell
      // theCoords[i+0][j+1] = badcoord;
      // theCoords[i+1][j+1] = badcoord;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Project coordinates
 */
// ----------------------------------------------------------------------

CoordinatesPtr project_coordinates(const CoordinatesPtr& theCoords,
                                   const Q& theQ,
                                   OGRSpatialReference& theSR)
{
  try
  {
    // Copy the original coordinates for projection
    auto coords = boost::make_shared<Coordinates>(*theCoords);

    // Create the coordinate transformation

    std::unique_ptr<OGRSpatialReference> src(new OGRSpatialReference);
    OGRErr err = src->SetFromUserInput(theQ->area().WKT().c_str());
    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP, "Unknown WKT in querydata: '" + theQ->area().WKT());

    // Clones the spatial reference object
    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(src.get(), &theSR));

    if (!transformation)
      throw Spine::Exception(
          BCP, "Failed to create the requested coordinate transformation during contouring");

    // Project the coordinates one at a time

    auto& c = *coords;  // avoid dereferencing the shared pointed all the time for speed
    auto nx = c.NX();
    auto ny = c.NY();

    for (std::size_t j = 0; j < ny; j++)
      for (std::size_t i = 0; i < nx; i++)
      {
        double x = c[i][j].X();
        double y = c[i][j].Y();
        if (!transformation->Transform(1, &x, &y))
        {
          x = kFloatMissing;
          y = kFloatMissing;
        }
        c[i][j] = NFmiPoint(x, y);
      }

    // If the target SR is geographic, we must discard the grid cells containing
    // the north or south poles since the cell vertex coordinates wrap around
    // the world. The more difficult alternative would be to divide the cell into
    // 4 triangles from the poles and contour the triangles.

    // We also have to check whether some grid cells cross the 180th meridian
    // and discard them

    if (theSR.IsGeographic())
    {
      const auto& grid = theQ->grid();
      auto northpole = grid.LatLonToGrid(0, 90);
      mark_cell_bad(c, northpole);
      auto southpole = grid.LatLonToGrid(0, -90);
      mark_cell_bad(c, southpole);

      NFmiPoint badcoord(kFloatMissing, kFloatMissing);
      for (std::size_t j = 0; j < ny; j++)
        for (std::size_t i = 0; i + 1 < nx; i++)
        {
          double lon1 = c[i][j].X();
          double lon2 = c[i + 1][j].X();
          if (lon1 != kFloatMissing && lon2 != kFloatMissing && std::abs(lon1 - lon2) > 180)
            c[i][j] = badcoord;
        }
    }

    return coords;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

CoordinatesPtr Engine::getWorldCoordinates(const Q& theQ, OGRSpatialReference* theSR) const
{
  try
  {
    // Hash value of unprojected and projected coordinates
    auto qhash = theQ->gridHashValue();
    auto projhash = qhash;

    if (theSR != nullptr)
    {
      boost::hash_combine(projhash, hash_value(*theSR));

      auto cached_coords = itsCoordinateCache.find(projhash);
      if (cached_coords)
        return cached_coords->get();
    }

    // Now we need either the original coordinates as is or need to project them

    auto cached_coords = itsCoordinateCache.find(qhash);

    if (cached_coords && theSR == nullptr)
      return cached_coords->get();

    if (!cached_coords)
    {
      auto ftr =
          boost::async([=] { return boost::make_shared<Coordinates>(get_world_xy(theQ)); }).share();
      itsCoordinateCache.insert(qhash, ftr);
      ftr.get();
      cached_coords = itsCoordinateCache.find(qhash);
    }

    // g++ 4.8.5 does not allow get to be called inside the lambda below, had to place it here

    auto coords = cached_coords->get();

    if (theSR == nullptr)
      return coords;

    // Project the coordinates

    auto ftr = boost::async([=] { return project_coordinates(coords, theQ, *theSR); }).share();

    itsCoordinateCache.insert(projhash, ftr);
    return ftr.get();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
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

ValuesPtr Engine::getValues(const Q& theQ,
                            std::size_t theValuesHash,
                            boost::posix_time::ptime theTime) const
{
  try
  {
    // If there is a future in the cache, ask it for the values

    auto values = itsValuesCache.find(theValuesHash);
    if (values)
      return values->get();

    // Else create a shared future for calculating the values
    auto ftr = boost::async([=] {
                 auto tmp = boost::make_shared<Values>();
                 theQ->values(*tmp, theTime);
                 return tmp;
               }).share();

    // Store the shared future into the cache for other threads to see too
    itsValuesCache.insert(theValuesHash, ftr);

    // And wait for the future to finish along with other threads
    return ftr.get();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::time_t Engine::getConfigModTime()
{
  auto repomanager = boost::atomic_load(&itsRepoManager);
  return repomanager->getConfigModTime();
}

int Engine::getLastConfigErrno()
{
  return lastConfigErrno;
}

}  // namespace QueryData
}  // namespace Engine
}  // namespace SmartMet

// DYNAMIC MODULE CREATION TOOLS

extern "C" void* engine_class_creator(const char* configfile, void* /* user_data */)
{
  return new SmartMet::Engine::Querydata::Engine(configfile);
}

extern "C" const char* engine_name()
{
  return "Querydata";
}
// ======================================================================
