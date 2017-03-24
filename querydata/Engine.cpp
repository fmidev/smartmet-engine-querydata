// ======================================================================
/*!
 * \brief Implementation of QEngine
 */
// ======================================================================

#include "Engine.h"
#include "RepoManager.h"
#include "Repository.h"
#include "Synchro.h"
#include "MetaQueryFilters.h"
#include <spine/Exception.h>

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
    : itsRepoManager(new RepoManager(configfile)),
      itsSynchro(),
      itsConfigFile(configfile),
      itsActiveThreadCount(0)
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

    itsRepoManager->init();
    itsSynchro.reset(new Synchronizer(this, itsConfigFile));

    // Wait until all initial data has been loaded
    while (!itsRepoManager->ready() && !itsShutdownRequested)
    {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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

    if (itsRepoManager != NULL)
      itsRepoManager->shutdown();

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
    if (itsRepoManager != NULL)
      itsRepoManager->shutdownRequestFlagSet();

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

    Spine::ReadLock lock(itsRepoManager->itsMutex);
    return itsRepoManager->itsProducerList;
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);
    return itsRepoManager->itsRepo.originTimes(producer);
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);
    return itsRepoManager->itsRepo.get(producer);
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);
    return itsRepoManager->itsRepo.get(producer, origintime);
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);
    return itsRepoManager->itsRepo.find(itsRepoManager->itsProducerList,
                                        itsRepoManager->itsProducerList,
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);
    return itsRepoManager->itsRepo.find(producerlist,
                                        itsRepoManager->itsProducerList,
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
	  Spine::ReadLock lock(itsRepoManager->itsMutex);
	  return itsRepoManager->validtimes(producer);
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);

    return itsRepoManager->itsRepo.getRepoContents(timeFormat, projectionFormat);
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);

    return itsRepoManager->itsRepo.getRepoMetadata();
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);

    return itsRepoManager->itsRepo.getRepoMetadata(theOptions);
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
      std::list<MetaData> repocontent;
      {
        Spine::ReadLock lock(itsRepoManager->itsMutex);

        repocontent = itsRepoManager->itsRepo.getRepoMetadata();
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

    if (syncProducers)
    {
      std::list<MetaData> repocontent;
      {
        Spine::ReadLock lock(itsRepoManager->itsMutex);

        repocontent = itsRepoManager->itsRepo.getRepoMetadata(options);
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
    Spine::ReadLock lock(itsRepoManager->itsMutex);

    return itsRepoManager->itsRepo.getSynchroInfos();
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
	  Spine::ReadLock lock(itsRepoManager->itsMutex);
	  return itsRepoManager->itsRepo.get(producer,starttime,endtime,timestep,origintime,timeinterpolation);
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
    return itsRepoManager->producerConfig(producer);
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

CoordinatesPtr project_coordinates(const Coordinates& theCoords,
                                   const Q& theQ,
                                   OGRSpatialReference& theSR)
{
  try
  {
    // Copy the original coordinates for projection
    auto coords = boost::make_shared<Coordinates>(theCoords);

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
    auto qhash = theQ->gridHashValue();

    // Handle native coordinates first
    if (theSR == nullptr)
    {
      auto cached_coords = itsCoordinateCache.find(qhash);
      if (cached_coords)
        return *cached_coords;

      auto coords = boost::make_shared<Coordinates>(get_world_xy(theQ));
      itsCoordinateCache.insert(qhash, coords);
      return coords;
    }

    // Must use a projected grid coordinates

    auto projhash = qhash;
    boost::hash_combine(projhash, hash_value(*theSR));

    // Use cached coordinates if possible
    auto cached_coords = itsCoordinateCache.find(projhash);
    if (cached_coords)
      return *cached_coords;

    // Must calculate cached coordinates from native coordinates
    cached_coords = itsCoordinateCache.find(qhash);
    if (!cached_coords)
    {
      cached_coords = boost::make_shared<Coordinates>(get_world_xy(theQ));
      itsCoordinateCache.insert(qhash, *cached_coords);
    }

    auto coords = project_coordinates(**cached_coords, theQ, *theSR);
    itsCoordinateCache.insert(projhash, coords);

    return coords;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

ValuesPtr Engine::getValues(const Q& theQ,
                            std::size_t theValuesHash,
                            boost::posix_time::ptime theTime) const
{
  try
  {
    auto values = itsValuesCache.find(theValuesHash);
    if (!values)
    {
      auto tmp = boost::make_shared<Values>();
      theQ->values(*tmp, theTime);
      itsValuesCache.insert(theValuesHash, tmp);
      return tmp;
    }
    return *values;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
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