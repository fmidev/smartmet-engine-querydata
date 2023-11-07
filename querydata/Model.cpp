// ======================================================================
/*!
 * \brief Data associated with a single model
 */
// ======================================================================

#include "Model.h"
#include "ValidPoints.h"
#include <boost/filesystem/operations.hpp>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGeoTools.h>
#include <newbase/NFmiQueryData.h>
#include <spine/Convenience.h>

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
             const std::string& validpointscachedir,
             Producer producer,
             std::string levelname,
             bool climatology,
             bool full,
             bool staticgrid,
             bool relativeuv,
             unsigned int update_interval,
             unsigned int minimum_expiration_time,
             bool mmap)
    : itsPath(filename),
      itsProducer(std::move(producer)),
      itsLevelName(std::move(levelname)),
      itsUpdateInterval(update_interval),
      itsMinimumExpirationTime(minimum_expiration_time),
      itsClimatology(climatology),
      itsFullGrid(full),
      itsStaticGrid(staticgrid),
      itsRelativeUV(relativeuv),
      itsValidTimeList(new ValidTimeList()),
      itsQueryData(new NFmiQueryData(filename.string(), mmap))
{
  try
  {
    if (!itsQueryData)
      throw Fmi::Exception(
          BCP, "Failed to initialize NFmiQueryData object from '" + filename.string() + "'!");

    itsOriginTime = itsQueryData->OriginTime();
    itsLoadTime = Fmi::SecondClock::universal_time();

    // May throw if file is gone
    itsModificationTime =
        boost::posix_time::from_time_t(boost::filesystem::last_write_time(filename));

    // Unique hash value for this model

    itsHashValue = 0;
    Fmi::hash_combine(itsHashValue, Fmi::hash_value(itsPath.string()));
    Fmi::hash_combine(itsHashValue, Fmi::hash_value(itsModificationTime));

    // querydata.conf changes may alter essential model properties
    Fmi::hash_combine(itsHashValue, Fmi::hash_value(itsClimatology));
    Fmi::hash_combine(itsHashValue, Fmi::hash_value(itsFullGrid));
    Fmi::hash_combine(itsHashValue, Fmi::hash_value(itsStaticGrid));
    Fmi::hash_combine(itsHashValue, Fmi::hash_value(itsRelativeUV));

    // We need an info object to intialize some data members

    boost::shared_ptr<NFmiFastQueryInfo> qinfo =
        boost::make_shared<NFmiFastQueryInfo>(itsQueryData.get());

    // Might as well pool it for subsequent use

    itsQueryInfoPool.push_front(qinfo);

    // This may be slow for huge data with missing values,
    // hence we configure separately whether this initialization
    // needs to be done or not. findvalidpoint acts accordingly.

    if (!itsFullGrid && !validpointscachedir.empty())
    {
      // Use grid hash for static grids, full hash otherwise
      auto hash = itsHashValue;
      if (itsStaticGrid)
      {
        hash = Fmi::hash_value(itsProducer);
        Fmi::hash_combine(hash, qinfo->GridHashValue());
        // Ignoring modification, path etc since the grid is static
      }

      itsValidPoints =
          boost::make_shared<ValidPoints>(itsProducer, itsPath, *qinfo, validpointscachedir, hash);
    }

    // Requesting the valid times repeatedly is slow if we have to do
    // a time conversion to Fmi::DateTime every time - hence we optimize

    auto& vt = *itsValidTimeList;
    for (qinfo->ResetTime(); qinfo->NextTime();)
    {
      const NFmiMetTime& t = qinfo->ValidTime();
      vt.push_back(t);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
      itsUpdateInterval(theModel.itsUpdateInterval),
      itsMinimumExpirationTime(theModel.itsMinimumExpirationTime),
      itsClimatology(theModel.itsClimatology),
      itsFullGrid(theModel.itsFullGrid),
      itsStaticGrid(theModel.itsStaticGrid),
      itsRelativeUV(theModel.itsRelativeUV),
      itsValidPoints(theModel.itsValidPoints),
      itsValidTimeList(theModel.itsValidTimeList),
      itsQueryData(std::move(theData))
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct a model without querydata file
 *
 * Note: The hash is given from the outside on purpose
 */
// ----------------------------------------------------------------------

Model::Model(boost::shared_ptr<NFmiQueryData> theData, std::size_t theHash)
    : itsHashValue(theHash), itsValidTimeList(new ValidTimeList()), itsQueryData(std::move(theData))
{
  try
  {
    // We need an info object to intialize some data members

    boost::shared_ptr<NFmiFastQueryInfo> qinfo =
        boost::make_shared<NFmiFastQueryInfo>(itsQueryData.get());

    // Might as well pool it for subsequent use

    itsQueryInfoPool.push_front(qinfo);

    // Requesting the valid times repeatedly is slow if we have to do
    // a time conversion to Fmi::DateTime every time - hence we optimize

    auto& vt = *itsValidTimeList;
    for (qinfo->ResetTime(); qinfo->NextTime();)
    {
      const NFmiMetTime& t = qinfo->ValidTime();
      vt.push_back(t);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Origin time accessor
 */
// ----------------------------------------------------------------------

const Fmi::DateTime& Model::originTime() const
{
  return itsOriginTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Load time accessor
 */
// ----------------------------------------------------------------------

const Fmi::DateTime& Model::loadTime() const
{
  return itsLoadTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Modification time accessor
 */
// ----------------------------------------------------------------------

const Fmi::DateTime& Model::modificationTime() const
{
  return itsModificationTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Estimated expiration time for products generated from this data
 */
// ----------------------------------------------------------------------

Fmi::DateTime Model::expirationTime() const
{
  // Expected time for the next model
  auto t1 = itsModificationTime + Fmi::Seconds(itsUpdateInterval);

  // Minimum expiration time from wall clock
  auto t2 = Fmi::SecondClock::universal_time() +
            Fmi::Seconds(itsMinimumExpirationTime);

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
 * \brief Return true if grid is full
 */
// ----------------------------------------------------------------------

bool Model::isFullGrid() const
{
  return itsFullGrid;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if grid is static, meaningful only if also not full
 */
// ----------------------------------------------------------------------

bool Model::isStaticGrid() const
{
  return itsStaticGrid;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if wind U/V components are relative to the grid
 */
// ----------------------------------------------------------------------

bool Model::isRelativeUV() const
{
  return itsRelativeUV;
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
      return {kFloatMissing, kFloatMissing};

    // If the model covers all grid points, we're done

    if (itsFullGrid)
    {
      NFmiPoint p = qi.LatLon();
      double distance = NFmiGeoTools::GeoDistance(latlon.X(), latlon.Y(), p.X(), p.Y());
      if (distance <= 1000 * maxdist)
        return p;
      return {kFloatMissing, kFloatMissing};
    }

    // The model does not cover the entire grid, but for example
    // only land or sea areas. We must search the nearest valid
    // model point.

    // Start an expanding search loop

    NFmiPoint bestpoint;
    bool ok = false;
    double bestdistance = maxdist * 1000;

    for (unsigned int y = 1;; y++)
    {
      int j = (2 * (y % 2) - 1) * (y >> 1);  // 0,-1,1,-2,2,-3,3... NOLINT(hicpp-signed-bitwise)

      NFmiPoint p = qi.PeekLocationLatLon(0, j);
      double distance = NFmiGeoTools::GeoDistance(latlon.X(), latlon.Y(), p.X(), p.Y());

      if (distance > bestdistance)
        break;

      for (unsigned int x = 1;; x++)
      {
        int i = (2 * (x % 2) - 1) * (x >> 1);  // 0,-1,1,-2,2,-3,3... NOLINT(hicpp-signed-bitwise)

        p = qi.PeekLocationLatLon(i, j);

        distance = NFmiGeoTools::GeoDistance(latlon.X(), latlon.Y(), p.X(), p.Y());

        if (distance > bestdistance)
          break;

        if (itsValidPoints && itsValidPoints->isvalid(qi.PeekLocationIndex(i, j)))
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
    return {kFloatMissing, kFloatMissing};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    auto qinfo = itsQueryInfoPool.front();
    itsQueryInfoPool.pop_front();
    qinfo->First();  // reset after prior use
    return qinfo;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Model::release(const boost::shared_ptr<NFmiFastQueryInfo>& theInfo) const
{
  try
  {
    Spine::WriteLock lock(itsQueryInfoPoolMutex);
    itsQueryInfoPool.emplace_front(theInfo);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
 * \brief Uncache related data
 */
// ----------------------------------------------------------------------

void Model::uncache() const
{
  if (itsValidPoints)
    itsValidPoints->uncache();
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

// ----------------------------------------------------------------------
/*!
 * \brief Set the LatLonCache for the querydata from external cache
 */
// ----------------------------------------------------------------------

void Model::setLatLonCache(const boost::shared_ptr<std::vector<NFmiPoint>>& theCache)
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

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
