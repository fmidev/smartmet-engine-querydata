// ======================================================================
/*!
 * \brief Data associated with a single model
 */
// ======================================================================

#include "Model.h"
#include "ValidPoints.h"
#include <boost/filesystem/operations.hpp>
#include <boost/functional/hash.hpp>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGeoTools.h>
#include <newbase/NFmiQueryData.h>
#include <spine/Exception.h>
#include <spine/Hash.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// ----------------------------------------------------------------------
/*!
 * \brief Construct a model
 *
 * Note that we could have ordered itsOriginTime after itsQueryData
 * in the object and then initialized the time in the constructor
 * list. This method is less risky against accidental reorders
 * in the header file.
 */
// ----------------------------------------------------------------------

Model::Model(const boost::filesystem::path& filename,
             const Producer& producer,
             const std::string& levelname,
             bool climatology,
             bool full,
             unsigned int update_interval,
             unsigned int minimum_expiration_time)
    : itsOriginTime(),
      itsPath(filename),
      itsProducer(producer),
      itsLevelName(levelname),
      itsUpdateInterval(update_interval),
      itsMinimumExpirationTime(minimum_expiration_time),
      itsClimatology(climatology),
      itsFullGrid(full),
      itsQueryData(new NFmiQueryData(filename.string())),
      itsValidPoints(),
      itsValidTimeList(new ValidTimeList()),
      itsQueryInfoPoolMutex(),
      itsQueryInfoPool()
{
  try
  {
    if (!itsQueryData)
      throw Spine::Exception(
          BCP, "Failed to initialize NFmiQueryData object from '" + filename.string() + "'!");

    itsOriginTime = itsQueryData->OriginTime();

    // May throw if file is gone
    itsModificationTime =
        boost::posix_time::from_time_t(boost::filesystem::last_write_time(filename));

    // We need an info object to intialize some data members

    boost::shared_ptr<NFmiFastQueryInfo> qinfo =
        boost::make_shared<NFmiFastQueryInfo>(itsQueryData.get());

    // Might as well pool it for subsequent use

    itsQueryInfoPool.push_front(qinfo);

    // Rereference for some extra speed

    auto& qi = *qinfo;

    // This may be slow for huge data with missing values,
    // hence we configure separately whether this initialization
    // needs to be done or not. findvalidpoint acts accordingly.

    if (!itsFullGrid)
      itsValidPoints.reset(new ValidPoints(qi));

    // Requesting the valid times repeatedly is slow if we have to do
    // a time conversion to ptime every time - hence we optimize

    auto& vt = *itsValidTimeList;
    for (qi.ResetTime(); qi.NextTime();)
    {
      const NFmiMetTime& t = qi.ValidTime();
      vt.push_back(t);
    }

    // Unique hash value for this model

    itsHashValue = 0;
    boost::hash_combine(itsHashValue, itsPath);
    boost::hash_combine(itsHashValue, itsModificationTime);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct a model from a filtered one
 */
// ----------------------------------------------------------------------

Model::Model(const Model& theModel, boost::shared_ptr<NFmiQueryData> theData, std::size_t theHash)
    : itsHashValue(theHash)  // decided externally on purpose
      ,
      itsOriginTime(theModel.itsOriginTime),
      itsPath(theModel.itsPath),
      itsModificationTime(theModel.itsModificationTime),
      itsProducer(theModel.itsProducer),
      itsLevelName(theModel.itsLevelName),
      itsClimatology(theModel.itsClimatology),
      itsFullGrid(true)  // we assume nearest valid point data is never needed for filtered data
      ,
      itsQueryData(theData),
      itsValidPoints(),
      itsValidTimeList(new ValidTimeList()),
      itsQueryInfoPoolMutex(),
      itsQueryInfoPool()
{
  try
  {
    // We need an info object to intialize some data members

    boost::shared_ptr<NFmiFastQueryInfo> qinfo =
        boost::make_shared<NFmiFastQueryInfo>(itsQueryData.get());

    // Might as well pool it for subsequent use

    itsQueryInfoPool.push_front(qinfo);

    // Rereference for some extra speed

    auto& qi = *qinfo;

    // As in the other constructor, just in case we decide we need valid point information anyway

    if (!itsFullGrid)
      itsValidPoints.reset(new ValidPoints(qi));

    // Requesting the valid times repeatedly is slow if we have to do
    // a time conversion to ptime every time - hence we optimize

    auto& vt = *itsValidTimeList;
    for (qi.ResetTime(); qi.NextTime();)
    {
      const NFmiMetTime& t = qi.ValidTime();
      vt.push_back(t);
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Origin time accessor
 */
// ----------------------------------------------------------------------

const boost::posix_time::ptime& Model::originTime() const
{
  return itsOriginTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Modification time accessor
 */
// ----------------------------------------------------------------------

const boost::posix_time::ptime& Model::modificationTime() const
{
  return itsModificationTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Estimated expiration time for products generated from this data
 */
// ----------------------------------------------------------------------

boost::posix_time::ptime Model::expirationTime() const
{
  // Expected time for the next model
  auto t1 = itsModificationTime + boost::posix_time::seconds(itsUpdateInterval);

  // Minimum expiration time from wall clock
  auto t2 = boost::posix_time::second_clock::universal_time() +
            boost::posix_time::seconds(itsMinimumExpirationTime);

  // Choose the later one. t1 dominates until the next model is overdue, in
  // which case we start waiting for it in smaller minimum expiration time
  // intervals. If the next model is early, too bad. Someone is bound to
  // make a fresh load of the data though, in which case the backend
  // will generate a new product and the frontend cache will be updated.

  return std::max(t1, t2);
}

// ----------------------------------------------------------------------
/*!
 * \brief Path accessor
 */
// ----------------------------------------------------------------------

const boost::filesystem::path& Model::path() const
{
  return itsPath;
}
// ----------------------------------------------------------------------
/*!
 * \brief Producer accessor
 */
// ----------------------------------------------------------------------

const Producer& Model::producer() const
{
  return itsProducer;
}

// ----------------------------------------------------------------------
/*!
 * \brief Level type accessor
 */
// ----------------------------------------------------------------------

const std::string& Model::levelName() const
{
  return itsLevelName;
}

// ----------------------------------------------------------------------
/*!
 * \brief Climatology accessor
 */
// ----------------------------------------------------------------------

bool Model::isClimatology() const
{
  return itsClimatology;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if grid ietoc smple
 */
// ----------------------------------------------------------------------

bool Model::isFullGrid() const
{
  return itsFullGrid;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find closest valid coordinate point within given radius (km)
 *
 * Returns (kFloatMissing,kFloatMissing) on failure
 *
 * TODO: REWRITE TO USE A NEARTREE OF VALID POINTS!!
 */
// ----------------------------------------------------------------------

NFmiPoint Model::validPoint(const NFmiPoint& latlon, double maxdist) const
{
  try
  {
    // First establish the nearest point
    NFmiFastQueryInfo qi(itsQueryData.get());

    if (!qi.NearestPoint(latlon) || !qi.IsGrid())
      return NFmiPoint(kFloatMissing, kFloatMissing);

    // If the model covers all grid points, we're done

    if (itsFullGrid)
    {
      NFmiPoint p = qi.LatLon();
      double distance = NFmiGeoTools::GeoDistance(latlon.X(), latlon.Y(), p.X(), p.Y());
      if (distance <= 1000 * maxdist)
        return p;
      else
        return NFmiPoint(kFloatMissing, kFloatMissing);
    }

    // The model does not cover the entire grid, but for example
    // only land or sea areas. We must search the nearest valid
    // model point.

    // Start an expanding search loop

    NFmiPoint bestpoint;
    bool ok = false;
    double bestdistance = maxdist * 1000;

    for (int y = 1;; y++)
    {
      int j = (2 * (y % 2) - 1) * (y >> 1);  // 0,-1,1,-2,2,-3,3...

      NFmiPoint p = qi.PeekLocationLatLon(0, j);
      double distance = NFmiGeoTools::GeoDistance(latlon.X(), latlon.Y(), p.X(), p.Y());

      if (distance > bestdistance)
        break;

      for (int x = 1;; x++)
      {
        int i = (2 * (x % 2) - 1) * (x >> 1);  // 0,-1,1,-2,2,-3,3...

        p = qi.PeekLocationLatLon(i, j);

        distance = NFmiGeoTools::GeoDistance(latlon.X(), latlon.Y(), p.X(), p.Y());

        if (distance > bestdistance)
          break;

        if (itsValidPoints->isvalid(qi.PeekLocationIndex(i, j)))
        {
          ok = true;
          bestpoint = p;
          bestdistance = distance;
        }
      }
    }

    // Check if we found any points within the search radius

    if (ok)
      return bestpoint;
    else
      return NFmiPoint(kFloatMissing, kFloatMissing);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return valid times available in the model
 */
// ----------------------------------------------------------------------

boost::shared_ptr<ValidTimeList> Model::validTimes() const
{
  return itsValidTimeList;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return a Q for the data
 */
// ----------------------------------------------------------------------

SharedInfo Model::info() const
{
  try
  {
    Spine::WriteLock lock(itsQueryInfoPoolMutex);
    if (itsQueryInfoPool.empty())
    {
      auto qinfo = boost::make_shared<NFmiFastQueryInfo>(itsQueryData.get());
      qinfo->First();  // reset for first use
      return qinfo;
    }
    else
    {
      auto qinfo = itsQueryInfoPool.front();
      itsQueryInfoPool.pop_front();
      qinfo->First();  // reset after prior use
      return qinfo;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void Model::release(boost::shared_ptr<NFmiFastQueryInfo> theInfo) const
{
  try
  {
    Spine::WriteLock lock(itsQueryInfoPoolMutex);
    itsQueryInfoPool.push_front(theInfo);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 *\ brief Return the hash value for the grid in the querydata
 */
// ----------------------------------------------------------------------

std::size_t Model::gridHashValue() const
{
  return itsQueryData->GridHashValue();
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the LatLonCache for the querydata from external cache
 */
// ----------------------------------------------------------------------

void Model::setLatLonCache(boost::shared_ptr<std::vector<NFmiPoint>> theCache)
{
  itsQueryData->SetLatLonCache(theCache);
}

// ----------------------------------------------------------------------
/*!
 * \brief Make querydata latlon cache and return it
 */
// ----------------------------------------------------------------------

boost::shared_ptr<std::vector<NFmiPoint>> Model::makeLatLonCache()
{
  return itsQueryData->LatLonCache();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return an unique hash for the object
 */
// ----------------------------------------------------------------------

std::size_t hash_value(const Model& theModel)
{
  return theModel.itsHashValue;
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
