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
#include <fmt/format.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <chrono>
#include <exception>
#include <ogr_spatialref.h>
#include <system_error>

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
    : itsRepoManager(boost::make_shared<RepoManager>(configfile)),
      itsConfigFile(configfile),
      itsActiveThreadCount(0),
      lastConfigErrno(EINPROGRESS)
{
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
    itsSynchro = boost::make_shared<Synchronizer>(this, itsConfigFile);

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

  while (!itsShutdownRequested)
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
    if (newfiletime != filetime && !itsShutdownRequested)
    {
      // File changed
      // Go into cooling period of waiting a few seconds and checking again
      // This assures there are no half completed writes
      lastConfigErrno = EINPROGRESS;

      try
      {
        while (newfiletime != filetime && !itsShutdownRequested)
        {
          std::cout << Spine::log_time_str() + " Querydata config " + itsConfigFile +
                           " updated, rereading\n";
          filetime = newfiletime;
          boost::this_thread::sleep_for(boost::chrono::seconds(3));
          newfiletime = boost::filesystem::last_write_time(itsConfigFile, ec);
        }

        if (!itsShutdownRequested)
        {
          // Generate new repomanager according to new configs
          boost::shared_ptr<RepoManager> newrepomanager =
              boost::make_shared<RepoManager>(itsConfigFile);

          // The old manager can be used to initialize common data faster
          auto oldrepomanager = boost::atomic_load(&itsRepoManager);
          newrepomanager->setOldManager(oldrepomanager);
          newrepomanager->init();

          // Wait until all initial data has been loaded
          while (!newrepomanager->ready() && !itsShutdownRequested)
          {
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
          }

          newrepomanager->removeOldManager();

          if (!itsShutdownRequested)
          {
            // Update current repomanager
            boost::atomic_store(&itsRepoManager, newrepomanager);
            std::cout << Spine::log_time_str() + " Querydata config " + itsConfigFile +
                             " update done\n";
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

void Engine::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (qengine)\n";
    auto repomanager = boost::atomic_load(&itsRepoManager);

    if (repomanager != nullptr)
      repomanager->shutdown();

    if (itsSynchro)
      itsSynchro->shutdown();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Engine::shutdownRequestFlagSet()
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    if (repomanager != nullptr)
      repomanager->shutdownRequestFlagSet();

    if (itsSynchro)
      itsSynchro->shutdownRequestFlagSet();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get caches sizes
 */
// ----------------------------------------------------------------------

CacheReportingStruct Engine::getCacheSizes() const
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given producer is defined
 */
// ----------------------------------------------------------------------

bool Engine::hasProducer(const Producer& producer) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);
    return repomanager->itsRepo.hasProducer(producer);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Return currently mapped files for a producer as table
 */
// ----------------------------------------------------------------------

Repository::ContentTable Engine::getEngineContents(const std::string& producer,
                                                   const std::string& timeFormat,
                                                   const std::string& projectionFormat) const
{
  try
  {
    auto repomanager = boost::atomic_load(&itsRepoManager);

    Spine::ReadLock lock(repomanager->itsMutex);

    if (producer.empty())
      return repomanager->itsRepo.getRepoContents(timeFormat, projectionFormat);
    return repomanager->itsRepo.getRepoContents(producer, timeFormat, projectionFormat);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    // Handle unknown producers such as observations quickly without exceptions
    if (!hasProducer(producer))
    {
      return {boost::posix_time::ptime(), boost::posix_time::hours(0)};  // is_null will return true
    }

    try
    {
      auto q = get(producer);
      auto validtimes = q->validTimes();
      if (validtimes->empty())
        return {boost::posix_time::ptime(),
                boost::posix_time::hours(0)};  // is_null will return true

      return {validtimes->front(), validtimes->back()};
    }
    catch (...)
    {
      return {boost::posix_time::ptime(), boost::posix_time::hours(0)};  // is_null will return true
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> Engine::getEngineSyncMetadata(const std::string& syncGroup) const
{
  try
  {
    auto syncProducers = itsSynchro->getSynchedData(syncGroup);

    if (!syncProducers)
      return std::list<MetaData>();  // Unknown sync group

    auto repomanager = boost::atomic_load(&itsRepoManager);

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<MetaData> Engine::getEngineSyncMetadata(const std::string& syncGroup,
                                                  const MetaQueryOptions& options) const
{
  try
  {
    auto syncProducers = itsSynchro->getSynchedData(syncGroup);
    auto repomanager = boost::atomic_load(&itsRepoManager);

    if (!syncProducers)
      return std::list<MetaData>();  // Unknown sync group

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t hash_value(const Fmi::SpatialReference& theSR)
{
  try
  {
    char* wkt;
    theSR.get()->exportToWkt(&wkt);
    std::string tmp(wkt);
    CPLFree(wkt);
    boost::hash<std::string> hasher;
    return hasher(tmp);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// TODO: Do we really need this anymore? Creating a CoordinateMatrix is quite fast
NFmiCoordinateMatrix get_world_xy(const Q& theQ)
{
  if (!theQ->isGrid())
    throw Spine::Exception(BCP, "Trying to contour non-gridded data");

  return theQ->CoordinateMatrix();
}

// ----------------------------------------------------------------------
/*!
 * \brief Mark the given coordinate cell as bad
 */
// ----------------------------------------------------------------------

void mark_cell_bad(NFmiCoordinateMatrix& theCoords, const NFmiPoint& theCoord)
{
  try
  {
    if (theCoord.X() == kFloatMissing || theCoord.Y() == kFloatMissing ||
        std::isnan(theCoord.X()) || std::isnan(theCoord.Y()))

      return;

    if (theCoord.X() >= 0 && theCoord.X() < theCoords.Width() - 1 && theCoord.Y() >= 0 &&
        theCoord.Y() < theCoords.Height() - 1)
    {
      auto i = static_cast<std::size_t>(theCoord.X());
      auto j = static_cast<std::size_t>(theCoord.Y());
      NFmiPoint badcoord(std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::quiet_NaN());
      theCoords(i + 0, j + 0) = badcoord;
      theCoords(i + 1, j + 0) = badcoord;
      // Marking two vertices bad is enough to invalidate the cell
      // theCoords(i+0,j+1])= badcoord;
      // theCoords(i+1],+1])= badcoord;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    auto coords = std::make_shared<NFmiCoordinateMatrix>(*theCoords);
    coords->Transform(transformation);

    // If the target SR is geographic, we must discard the grid cells containing
    // the north or south poles since the cell vertex coordinates wrap around
    // the world. The more difficult alternative would be to divide the cell into
    // 4 triangles from the poles and contour the triangles.

    // We also have to check whether some grid cells cross the 180th meridian
    // and discard them

    // If the target SR is no geographic, we discard all very elongated cells
    // since they are likely spanning the world

    NFmiPoint badcoord(std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::quiet_NaN());

    if (theSR.IsGeographic() != 0)
    {
      auto& c = *coords;

      const auto& grid = theQ->grid();
      auto northpole = grid.LatLonToGrid(0, 90);
      mark_cell_bad(c, northpole);
      auto southpole = grid.LatLonToGrid(0, -90);
      mark_cell_bad(c, southpole);

      const auto nx = c.Width();
      const auto ny = c.Height();

      for (std::size_t j = 0; j < ny; j++)
        for (std::size_t i = 0; i + 1 < nx; i++)
        {
          double lon1 = c.X(i, j);
          double lon2 = c.X(i + 1, j);
          if (lon1 != kFloatMissing && lon2 != kFloatMissing && std::abs(lon1 - lon2) > 180)
            c.Set(i, j, badcoord);
        }
    }

    else
    {
      auto& c = *coords;

      const auto nx = c.Width();
      const auto ny = c.Height();

      for (std::size_t j = 0; j + 1 < ny; j++)
        for (std::size_t i = 0; i + 1 < nx; i++)
        {
          // Checking one diagonal should be sufficient, but to make sure
          // one could maybe check the other diagonal too. Insufficient information
          // on different types of problems.

          double x1 = c.X(i, j);
          double x2 = c.X(i + 1, j + 1);
          double y1 = c.Y(i, j);
          double y2 = c.Y(i + 1, j + 1);
          double dx = std::abs(x2 - x1);
          double dy = std::abs(y2 - y1);
          if (dx != 0)
          {
            if (dx / dy > 100 || dx / dy < 0.01)
              c.Set(i, j, badcoord);
          }
          else if (dy != 0)
          {
            if (dy / dx > 100 || dy / dx < 0.01)
              c.Set(i, j, badcoord);
          }
          else
            c.Set(i, j, badcoord);  // zero length diagonal not allowed
        }
    }

    return coords;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

CoordinatesPtr Engine::getWorldCoordinates(const Q& theQ, const Fmi::SpatialReference& theSR) const
{
  try
  {
    // Hash value of original WorldXY coordinates
    auto qhash = theQ->gridHashValue();

    // Hash value of projected coordinates
    auto projhash = qhash;

    if (theSR != nullptr)
    {
      // Return original world XY directly with get_world_xy if spatial
      // references match This is absolutely necessary to avoid gaps in
      // WMS tiles since with proj(invproj(p)) may differ significantly
      // from p outside the valid area of the projection.

      auto datawkt = theQ->info()->Area()->WKT();
      auto reqwkt = Fmi::OGR::exportToWkt(*theSR);

      if (datawkt != reqwkt)
        boost::hash_combine(projhash, hash_value(*theSR));
    }

    // Search cache for the projected coordinates or WorldXY coordinates
    auto cached_coords = itsCoordinateCache.find(projhash);
    if (cached_coords)
      return cached_coords->get();

    // Now we need to to get WorldXY coordinates. We cache the result so that it does not
    // have to be calculated again - even though it is a fast calculation.

    auto ftr =
        std::async([&] { return std::make_shared<NFmiCoordinateMatrix>(get_world_xy(theQ)); })
            .share();
    itsCoordinateCache.insert(qhash, ftr);
    auto worldxy = ftr.get();

    // Return WorldXY if so requested

    if (qhash == projhash)
      return worldxy;

    // Project to target SR. Do NOT use intermediate latlons in any datum, or the Z value will not
    // be included in all stages of the projection, and large errors will occur if the datums
    // differ significantly (e.g. sphere vs ellipsoid)

    // g++ 4.8.5 does not allow get to be called inside the lambda below, had to place it here

    // Project the coordinates

    auto ftr2 = std::async([&] { return project_coordinates(worldxy, theQ, theSR); }).share();

    itsCoordinateCache.insert(projhash, ftr2);
    return ftr2.get();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    auto ftr = std::async(std::launch::async,
                          [&] { return std::make_shared<Values>(theQ->values(theTime)); })
                   .share();

    // Store the shared future into the cache for other threads to see too
    itsValuesCache.insert(theValuesHash, ftr);

    // And wait for the future to finish along with other threads
    return ftr.get();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to retrieve data")
        .addParameter("time", Fmi::to_iso_extended_string(theTime));
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
                            const Spine::Parameter& theParam,
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
    auto ftr = std::async(std::launch::async,
                          [&] { return std::make_shared<Values>(theQ->values(theParam, theTime)); })
                   .share();

    // Store the shared future into the cache for other threads to see too
    itsValuesCache.insert(theValuesHash, ftr);

    // And wait for the future to finish along with other threads
    return ftr.get();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to retrieve data")
        .addParameter("time", Fmi::to_iso_extended_string(theTime));
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

}  // namespace Querydata
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
