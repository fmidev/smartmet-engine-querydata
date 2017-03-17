#include "Q.h"
#include "Model.h"

#include <spine/ParameterFactory.h>
#include <spine/Exception.h>

#include <gis/Box.h>
#include <gis/DEM.h>
#include <gis/LandCover.h>

#include <macgyver/Astronomy.h>
#include <macgyver/CharsetTools.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeZoneFactory.h>
#include <macgyver/TimeFormatter.h>

#include <newbase/NFmiGdalArea.h>
#include <newbase/NFmiMetMath.h>
#include <newbase/NFmiMultiQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeList.h>

#include <gdal/ogr_spatialref.h>

#include <boost/date_time/time_facet.hpp>
#include <boost/date_time/local_time/local_time_io.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/timer/timer.hpp>

#include <cassert>
#include <stdexcept>

namespace ts = SmartMet::Spine::TimeSeries;

namespace
{
const char *LevelName(FmiLevelType theLevel)
{
  try
  {
    switch (theLevel)
    {
      case kFmiGroundSurface:
        return "GroundSurface";
      case kFmiPressureLevel:
        return "PressureLevel";
      case kFmiMeanSeaLevel:
        return "MeanSeaLevel";
      case kFmiAltitude:
        return "Altitude";
      case kFmiHeight:
        return "Height";
      case kFmiHybridLevel:
        return "HybridLevel";
      case kFmi:
        return "?";
      case kFmiAnyLevelType:
        return "AnyLevelType";
      case kFmiRoadClass1:
        return "RoadClass1";
      case kFmiRoadClass2:
        return "RoadClass2";
      case kFmiRoadClass3:
        return "RoadClass3";
      case kFmiSoundingLevel:
        return "SoundingLevel";
      case kFmiAmdarLevel:
        return "AmdarLevel";
      case kFmiFlightLevel:
        return "FlightLevel";
      case kFmiDepth:
        return "Depth";
      case kFmiNoLevelType:
        return "NoLevel";
#ifndef UNREACHABLE
      default:
        throw SmartMet::Spine::Exception(BCP, "Internal error in deducing level names");
#endif
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// Max interpolation gap
const int maxgap = 6 * 60;

// ----------------------------------------------------------------------
/*!
 * \brief Is the location of water type?
 */
// ----------------------------------------------------------------------

bool iswater(const Spine::Location &theLocation)
{
  try
  {
    if (theLocation.dem == 0)
      return true;

    return Fmi::LandCover::isOpenWater(theLocation.covertype);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The destructor releases the NFmiFastQueryInfo back into a pool
 */
// ----------------------------------------------------------------------

QImpl::~QImpl()
{
  try
  {
    for (std::size_t i = 0; i < itsInfos.size(); i++)
      itsModels[i]->release(itsInfos[i]);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct from a single model
 */
// ----------------------------------------------------------------------

QImpl::QImpl(SharedModel theModel)
{
  try
  {
    itsModels.push_back(theModel);
    itsInfos.push_back(theModel->info());
    itsInfo = theModel->info();

    itsValidTimes = theModel->validTimes();

    itsHashValue = hash_value(theModel);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct from multiple models
 */
// ----------------------------------------------------------------------

QImpl::QImpl(const std::vector<SharedModel> &theModels)
    : itsModels(theModels), itsInfos(), itsValidTimes(new ValidTimeList)
{
  try
  {
    if (theModels.empty())
      throw Spine::Exception(BCP, "Cannot initialize any empty view over multiple models");

    for (std::size_t i = 0; i < itsModels.size(); i++)
      itsInfos.push_back(itsModels[i]->info());

    if (itsInfos.size() > 1)
      itsInfo.reset(new NFmiMultiQueryInfo(itsInfos));
    else
      itsInfo = itsInfos[0];

    // Establish hash value
    itsHashValue = 0;
    BOOST_FOREACH (const auto &model, itsModels)
    {
      boost::hash_combine(itsHashValue, model);
    }

    // Establish unique valid times
    std::set<boost::posix_time::ptime> uniquetimes;
    for (std::size_t i = 0; i < itsModels.size(); i++)
    {
      const auto &validtimes = itsModels[i]->validTimes();
      BOOST_FOREACH (const auto &t, *validtimes)
        uniquetimes.insert(t);
    }
    BOOST_FOREACH (const auto &t, uniquetimes)
      itsValidTimes->push_back(t);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Direct access to the data
 */
// ----------------------------------------------------------------------

boost::shared_ptr<NFmiFastQueryInfo> QImpl::info()
{
  return itsInfo;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return metadata on the querydata
 */
// ----------------------------------------------------------------------

MetaData QImpl::metaData()
{
  try
  {
    MetaData meta;

    // TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO
    NFmiFastQueryInfo &qi = *itsInfo;

    meta.producer = itsModels[0]->producer();

    // Get querydata origintime

    meta.originTime = qi.OriginTime();

    // Get querydata first time
    if (qi.FirstTime())
      meta.firstTime = qi.ValidTime();
    else
      meta.firstTime = boost::posix_time::not_a_date_time;

    // Get querydata last time
    if (qi.LastTime())
      meta.lastTime = qi.ValidTime();
    else
      meta.lastTime = boost::posix_time::not_a_date_time;

    // Get querydata timestep
    if (qi.FirstTime() && qi.NextTime())
    {
      qi.FirstTime();
      NFmiTime t1 = qi.ValidTime();
      qi.NextTime();
      NFmiTime t2 = qi.ValidTime();
      meta.timeStep = t2.DifferenceInMinutes(t1);
    }
    else
    {
      meta.timeStep = 0;
    }

    // Get querydata timesteps size
    meta.nTimeSteps = qi.SizeTimes();

    // Get the parameter list from querydatainfo
    std::list<ModelParameter> params;
    for (qi.ResetParam(); qi.NextParam(false);)
    {
      const int paramID = boost::numeric_cast<int>(qi.Param().GetParamIdent());
      const std::string paramName = Spine::ParameterFactory::instance().name(paramID);
      const std::string paramDesc = qi.Param().GetParamName().CharPtr();
      const std::string paramPrec = qi.Param().GetParam()->Precision().CharPtr();
      // Find the numerical part of the precision string
      auto dot = paramPrec.find(".");
      auto fchar = paramPrec.find("f");
      if ((dot != std::string::npos) && (fchar != std::string::npos))
      {
        auto theNumber = std::string(paramPrec.begin() + dot + 1, paramPrec.begin() + fchar);
        params.emplace_back(paramName, paramDesc, std::strtol(theNumber.c_str(), NULL, 10));
      }
      else
      {
        params.emplace_back(paramName, paramDesc, 0);  // 0 is the default
      }
    }

    // Get the model level list from querydatainfo
    std::list<ModelLevel> levels;
    qi.ResetLevel();
    while (qi.NextLevel())
    {
      const NFmiLevel &level = *qi.Level();

      auto type = ::LevelName(level.LevelType());
      auto name = level.GetName().CharPtr();
      auto value = level.LevelValue();
      levels.emplace_back(type, name, value);
    }

    meta.levels = levels;
    meta.parameters = params;

    // Get projection string
    if (qi.Area() == NULL)
    {
      meta.WKT = "nan";
      return meta;
    }
    else
    {
      meta.WKT = qi.Area()->WKT();
    }

    // Get querydata area info

    const NFmiArea *area = qi.Area();
    const NFmiGrid *grid = qi.Grid();

    meta.ullon = area->TopLeftLatLon().X();
    meta.ullat = area->TopLeftLatLon().Y();
    meta.urlon = area->TopRightLatLon().X();
    meta.urlat = area->TopRightLatLon().Y();
    meta.bllon = area->BottomLeftLatLon().X();
    meta.bllat = area->BottomLeftLatLon().Y();
    meta.brlon = area->BottomRightLatLon().X();
    meta.brlat = area->BottomRightLatLon().Y();
    meta.clon = area->CenterLatLon().X();
    meta.clat = area->CenterLatLon().Y();

    // Get querydata grid info

    meta.xNumber = boost::numeric_cast<unsigned int>(grid->XNumber());
    meta.yNumber = boost::numeric_cast<unsigned int>(grid->YNumber());

    meta.xResolution = area->WorldXYWidth() / grid->XNumber() / 1000.0;
    meta.yResolution = area->WorldXYHeight() / grid->YNumber() / 1000.0;

    meta.areaWidth = area->WorldXYWidth() / 1000.0;
    meta.areaHeight = area->WorldXYHeight() / 1000.0;

    meta.aspectRatio = area->WorldXYAspectRatio();

    meta.wgs84Envelope = getWGS84Envelope();

    return meta;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

const WGS84Envelope &QImpl::getWGS84Envelope()
{
  Spine::UpgradeReadLock readlock(itsWGS84EnvelopeMutex);

  if (itsWGS84Envelope)
    return *itsWGS84Envelope;

  Spine::UpgradeWriteLock writelock(readlock);

  if (not itsWGS84Envelope)
    itsWGS84Envelope.reset(new WGS84Envelope(itsInfo));
  return *itsWGS84Envelope;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return origin time of the model
 */
// ----------------------------------------------------------------------

const NFmiMetTime &QImpl::originTime() const
{
  try
  {
    return itsInfo->OriginTime();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Return modification time of the model
 */
// ----------------------------------------------------------------------

boost::posix_time::ptime QImpl::modificationTime() const
{
  try
  {
    auto t = itsModels[0]->modificationTime();

    for (std::size_t i = 1; i < itsModels.size(); i++)
    {
      t = std::max(t, itsModels[i]->modificationTime());
    }

    return t;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return valid times of the model
 */
// ----------------------------------------------------------------------

boost::shared_ptr<ValidTimeList> QImpl::validTimes() const
{
  return itsValidTimes;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the level type of the model
 */
// ----------------------------------------------------------------------

const std::string &QImpl::levelName() const
{
  try
  {
    return itsModels[0]->levelName();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the level type of the model
 */
// ----------------------------------------------------------------------

FmiLevelType QImpl::levelType() const
{
  try
  {
    return itsInfo->LevelType();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the data represents a climatology
 */
// ----------------------------------------------------------------------

bool QImpl::isClimatology() const
{
  try
  {
    return itsModels[0]->isClimatology();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the data covers the entire grid
 */
// ----------------------------------------------------------------------

bool QImpl::isFullGrid() const
{
  try
  {
    return itsModels[0]->isFullGrid();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the nearest grid point with valid data
 */
// ----------------------------------------------------------------------

NFmiPoint QImpl::validPoint(const NFmiPoint &theLatLon, double theMaxDist) const
{
  try
  {
    return itsModels[0]->validPoint(theLatLon, theMaxDist);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Reset the time iterator
 */
// ----------------------------------------------------------------------

void QImpl::resetTime()
{
  try
  {
    itsInfo->ResetTime();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the first time
 */
// ----------------------------------------------------------------------

bool QImpl::firstTime()
{
  try
  {
    return itsInfo->FirstTime();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the last time
 */
// ----------------------------------------------------------------------

bool QImpl::lastTime()
{
  try
  {
    return itsInfo->LastTime();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Advance the time iterator
 */
// ----------------------------------------------------------------------

bool QImpl::nextTime()
{
  try
  {
    return itsInfo->NextTime();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Previous time position
 */
// ----------------------------------------------------------------------

bool QImpl::previousTime()
{
  try
  {
    return itsInfo->PreviousTime();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the time iterator is valid
 */
// ----------------------------------------------------------------------

bool QImpl::isTimeUsable() const
{
  try
  {
    return itsInfo->IsTimeUsable();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the valid time
 */
// ----------------------------------------------------------------------

const NFmiMetTime &QImpl::validTime() const
{
  try
  {
    return itsInfo->ValidTime();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the valid time
 */
// ----------------------------------------------------------------------

bool QImpl::time(const NFmiMetTime &theTime)
{
  try
  {
    return itsInfo->Time(theTime);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*
 *! \brief Set the given parameter
 */
// ----------------------------------------------------------------------

bool QImpl::param(FmiParameterName theParam)
{
  try
  {
    return itsInfo->Param(theParam);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Reset the parameter iterator
 */
// ----------------------------------------------------------------------

void QImpl::resetParam()
{
  try
  {
    itsInfo->ResetParam();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Advance the parameter iterator
 */
// ----------------------------------------------------------------------

bool QImpl::nextParam(bool ignoreSubParams)
{
  try
  {
    return itsInfo->NextParam(ignoreSubParams);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the data is gridded
 */
// ----------------------------------------------------------------------

bool QImpl::isArea() const
{
  try
  {
    return (itsInfo->Area() != NULL);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the data is gridded
 */
// ----------------------------------------------------------------------

bool QImpl::isGrid() const
{
  try
  {
    return (itsInfo->Grid() != NULL);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the area
 */
// ----------------------------------------------------------------------

const NFmiArea &QImpl::area() const
{
  try
  {
    assert(itsInfo->Area() != NULL);
    return *itsInfo->Area();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the grid
 */
// ----------------------------------------------------------------------

const NFmiGrid &QImpl::grid() const
{
  try
  {
    assert(itsInfo->Grid() != NULL);
    return *itsInfo->Grid();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the param
 */
// ----------------------------------------------------------------------

const NFmiDataIdent &QImpl::param() const
{
  try
  {
    return itsInfo->Param();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the level
 */
// ----------------------------------------------------------------------

const NFmiLevel &QImpl::level() const
{
  try
  {
    return *itsInfo->Level();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the given point is in the data
 */
// ----------------------------------------------------------------------

bool QImpl::isInside(double theLon, double theLat, double theMaxDist)
{
  try
  {
    return itsInfo->IsInside(NFmiPoint(theLon, theLat), 1000 * theMaxDist);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return current parameter
 */
// ----------------------------------------------------------------------

FmiParameterName QImpl::parameterName() const
{
  try
  {
    return FmiParameterName(itsInfo->Param().GetParamIdent());
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Interpolate the value
 */
// ----------------------------------------------------------------------

float QImpl::interpolate(const NFmiPoint &theLatLon,
                         const NFmiMetTime &theTime,
                         int theMaxMinuteGap)
{
  try
  {
    return itsInfo->InterpolatedValue(theLatLon, theTime, theMaxMinuteGap);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Reset the level iterator
 */
// ----------------------------------------------------------------------

void QImpl::resetLevel()
{
  try
  {
    itsInfo->ResetLevel();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the first level
 */
// ----------------------------------------------------------------------

bool QImpl::firstLevel()
{
  try
  {
    return itsInfo->FirstLevel();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Advance the level iterator
 */
// ----------------------------------------------------------------------

bool QImpl::nextLevel()
{
  try
  {
    return itsInfo->NextLevel();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the level value
 */
// ----------------------------------------------------------------------

float QImpl::levelValue() const
{
  try
  {
    return itsInfo->Level()->LevelValue();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the coordinates for the given location index
 */
// ----------------------------------------------------------------------

NFmiPoint QImpl::latLon(long theIndex) const
{
  try
  {
    return itsInfo->LatLon(theIndex);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the version number
 */
// ----------------------------------------------------------------------

double QImpl::infoVersion() const
{
  try
  {
    return itsInfo->InfoVersion();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the parameter index
 */
// ----------------------------------------------------------------------

unsigned long QImpl::paramIndex() const
{
  try
  {
    return itsInfo->ParamIndex();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Set the parameter index
 */
// ----------------------------------------------------------------------

bool QImpl::paramIndex(unsigned long theIndex)
{
  try
  {
    return itsInfo->ParamIndex(theIndex);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the level index
 */
// ----------------------------------------------------------------------

unsigned long QImpl::levelIndex() const
{
  try
  {
    return itsInfo->LevelIndex();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Set the level index
 */
// ----------------------------------------------------------------------

bool QImpl::levelIndex(unsigned long theIndex)
{
  try
  {
    return itsInfo->LevelIndex(theIndex);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the time index
 */
// ----------------------------------------------------------------------

unsigned long QImpl::timeIndex() const
{
  try
  {
    return itsInfo->TimeIndex();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the time index
 */
// ----------------------------------------------------------------------

bool QImpl::timeIndex(unsigned long theIndex)
{
  try
  {
    return itsInfo->TimeIndex(theIndex);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the location index
 */
// ----------------------------------------------------------------------

unsigned long QImpl::locationIndex() const
{
  try
  {
    return itsInfo->LocationIndex();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the location index
 */
// ----------------------------------------------------------------------

bool QImpl::locationIndex(unsigned long theIndex)
{
  try
  {
    return itsInfo->LocationIndex(theIndex);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Prepare cache values for speeding up time interpolation
 */
// ----------------------------------------------------------------------

bool QImpl::calcTimeCache(NFmiQueryInfo &theTargetInfo, checkedVector<NFmiTimeCache> &theTimeCache)
{
  try
  {
    return itsInfo->CalcTimeCache(theTargetInfo, theTimeCache);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Prepare cache values for speeding up time interpolation
 */
// ----------------------------------------------------------------------

NFmiTimeCache QImpl::calcTimeCache(const NFmiMetTime &theTime)
{
  try
  {
    return itsInfo->CalcTimeCache(theTime);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform cached interpolation
 */
// ----------------------------------------------------------------------

float QImpl::cachedInterpolation(const NFmiTimeCache &theTimeCache)
{
  try
  {
    return itsInfo->CachedInterpolation(theTimeCache);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform cached interpolation
 */
// ----------------------------------------------------------------------

float QImpl::cachedInterpolation(const NFmiLocationCache &theLocationCache)
{
  try
  {
    return itsInfo->CachedInterpolation(theLocationCache);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform cached interpolation
 */
// ----------------------------------------------------------------------

float QImpl::cachedInterpolation(const NFmiLocationCache &theLocationCache,
                                 const NFmiTimeCache &theTimeCache)
{
  try
  {
    return itsInfo->CachedInterpolation(theLocationCache, theTimeCache);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform landscaped cached interpolation
 */
// ----------------------------------------------------------------------

void QImpl::landscapeCachedInterpolation(NFmiDataMatrix<float> &theMatrix,
                                         const NFmiDataMatrix<NFmiLocationCache> &theLocationCache,
                                         const NFmiTimeCache &theTimeCache,
                                         const NFmiDataMatrix<float> &theDEMValues,
                                         const NFmiDataMatrix<bool> &theWaterFlags)
{
  try
  {
    itsInfo->LandscapeCachedInterpolation(
        theMatrix, theLocationCache, theTimeCache, theDEMValues, theWaterFlags);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform fast interpolation with cached location information
 */
// ----------------------------------------------------------------------

bool QImpl::calcLatlonCachePoints(NFmiQueryInfo &theTargetInfo,
                                  NFmiDataMatrix<NFmiLocationCache> &theLocationCache)
{
  try
  {
    return itsInfo->CalcLatlonCachePoints(theTargetInfo, theLocationCache);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Interpolate values
 */
// ----------------------------------------------------------------------

void QImpl::values(NFmiDataMatrix<float> &theMatrix,
                   const NFmiDataMatrix<float> &theDEMValues,  // DEM values for landscaping (an
                                                               // empty matrix by default)
                   const NFmiDataMatrix<bool> &theWaterFlags   // Water flags for landscaping (an
                                                               // empty matrix by default)
                   )
{
  try
  {
    if ((theDEMValues.NX() > 0) && (theWaterFlags.NX() > 0))
      itsInfo->LandscapeValues(theMatrix, theDEMValues, theWaterFlags);
    else
      itsInfo->Values(theMatrix);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Interpolate values
 */
// ----------------------------------------------------------------------

void QImpl::values(NFmiDataMatrix<float> &theMatrix,
                   const NFmiMetTime &theInterpolatedTime,
                   const NFmiDataMatrix<float> &theDEMValues,  // DEM values for landscaping (an
                                                               // empty matrix by default)
                   const NFmiDataMatrix<bool> &theWaterFlags   // Water flags for landscaping (an
                                                               // empty matrix by default)
                   )
{
  try
  {
    if ((theDEMValues.NX() > 0) && (theWaterFlags.NX() > 0))
      itsInfo->LandscapeValues(theMatrix, theInterpolatedTime, theDEMValues, theWaterFlags);
    else
      itsInfo->Values(theMatrix, theInterpolatedTime);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Interpolate values
 */
// ----------------------------------------------------------------------

void QImpl::values(const NFmiDataMatrix<NFmiPoint> &theLatlonMatrix,
                   NFmiDataMatrix<float> &theValues,
                   const NFmiMetTime &theTime,
                   float P,
                   float H)
{
  try
  {
    itsInfo->Values(theLatlonMatrix, theValues, theTime, P, H);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract subgrid values
 */
// ----------------------------------------------------------------------

void QImpl::croppedValues(NFmiDataMatrix<float> &theMatrix,
                          int x1,
                          int y1,
                          int x2,
                          int y2,
                          const NFmiDataMatrix<float> &theDEMValues,  // DEM values for landscaping
                                                                      // (an empty matrix by
                                                                      // default)
                          const NFmiDataMatrix<bool> &theWaterFlags   // Water flags for landscaping
                          // (an empty matrix by default)
                          ) const
{
  try
  {
    if ((theDEMValues.NX() > 0) && (theWaterFlags.NX() > 0))
      itsInfo->LandscapeCroppedValues(theMatrix, x1, y1, x2, y2, theDEMValues, theWaterFlags);
    else
      itsInfo->CroppedValues(theMatrix, x1, y1, x2, y2);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values for pressure level
 */
// ----------------------------------------------------------------------

void QImpl::pressureValues(NFmiDataMatrix<float> &theValues,
                           const NFmiMetTime &theInterpolatedTime,
                           float wantedPressureLevel)
{
  try
  {
    return itsInfo->PressureValues(theValues, theInterpolatedTime, wantedPressureLevel);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values for pressure level
 */
// ----------------------------------------------------------------------

void QImpl::pressureValues(NFmiDataMatrix<float> &theValues,
                           const NFmiGrid &theWantedGrid,
                           const NFmiMetTime &theInterpolatedTime,
                           float wantedPressureLevel)
{
  try
  {
    return itsInfo->PressureValues(
        theValues, theWantedGrid, theInterpolatedTime, wantedPressureLevel);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the lat lon cache
 */
// ----------------------------------------------------------------------

boost::shared_ptr<std::vector<NFmiPoint>> QImpl::latLonCache() const
{
  try
  {
    return itsInfo->RefQueryData()->LatLonCache();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

//----------------------------------------------------------------------
/*!
 * \brief Return status on whether a sub parameter is active
 */
// ----------------------------------------------------------------------

bool QImpl::isSubParamUsed() const
{
  try
  {
    return itsInfo->IsSubParamUsed();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
//----------------------------------------------------------------------
/*!
 * \brief Restore status on whether a sub parameter is active
 */
// ----------------------------------------------------------------------

void QImpl::setIsSubParamUsed(bool theState)
{
  try
  {
    itsInfo->SetIsSubParamUsed(theState);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Time formatter
 */
// ----------------------------------------------------------------------

std::string format_date(const boost::local_time::local_date_time &ldt,
                        const std::locale &llocale,
                        const std::string &fmt)
{
  try
  {
    typedef boost::date_time::time_facet<boost::local_time::local_date_time, char> tfacet;
    std::ostringstream os;
    os.imbue(std::locale(llocale, new tfacet(fmt.c_str())));
    os << ldt;
    return Fmi::latin1_to_utf8(os.str());
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindCompass 8th
 */
// ----------------------------------------------------------------------

ts::Value WindCompass8(QImpl &q,
                       const Spine::Location &loc,
                       const boost::local_time::local_date_time &ldt)
{
  try
  {
    static const char *names[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

    if (!q.param(kFmiWindDirection))
      return Spine::TimeSeries::None();

    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (value == kFloatMissing)
      return Spine::TimeSeries::None();

    int i = static_cast<int>((value + 22.5) / 45) % 8;
    return names[i];
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindCompass 16th
 */
// ----------------------------------------------------------------------

ts::Value WindCompass16(QImpl &q,
                        const Spine::Location &loc,
                        const boost::local_time::local_date_time &ldt)
{
  try
  {
    static const char *names[] = {"N",
                                  "NNE",
                                  "NE",
                                  "ENE",
                                  "E",
                                  "ESE",
                                  "SE",
                                  "SSE",
                                  "S",
                                  "SSW",
                                  "SW",
                                  "WSW",
                                  "W",
                                  "WNW",
                                  "NW",
                                  "NNW"};

    if (!q.param(kFmiWindDirection))
      return Spine::TimeSeries::None();

    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (value == kFloatMissing)
      return Spine::TimeSeries::None();

    int i = static_cast<int>((value + 11.25) / 22.5) % 16;
    return names[i];
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindCompass 32th
 */
// ----------------------------------------------------------------------

ts::Value WindCompass32(QImpl &q,
                        const Spine::Location &loc,
                        const boost::local_time::local_date_time &ldt)
{
  try
  {
    static const char *names[] = {"N", "NbE", "NNE", "NEbN", "NE", "NEbE", "ENE", "EbN",
                                  "E", "EbS", "ESE", "SEbE", "SE", "SEbS", "SSE", "SbE",
                                  "S", "SbW", "SSW", "SWbS", "SW", "SWbW", "WSW", "WbS",
                                  "W", "WbN", "WNW", "NWbW", "NW", "NWbN", "NNW", "NbW"};

    if (!q.param(kFmiWindDirection))
      return Spine::TimeSeries::None();

    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (value == kFloatMissing)
      return Spine::TimeSeries::None();

    int i = static_cast<int>((value + 5.625) / 11.25) % 32;
    return names[i];
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Cloudiness8th
 */
// ----------------------------------------------------------------------

ts::Value Cloudiness8th(QImpl &q,
                        const Spine::Location &loc,
                        const boost::local_time::local_date_time &ldt)
{
  try
  {
    if (!q.param(kFmiTotalCloudCover))
      return Spine::TimeSeries::None();

    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (value == kFloatMissing)
      return Spine::TimeSeries::None();

    // This is the synoptic interpretation of 8s

    int n = boost::numeric_cast<int>(ceil(value / 12.5));
    return n;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindChill
 */
// ----------------------------------------------------------------------

ts::Value WindChill(QImpl &q,
                    const Spine::Location &loc,
                    const boost::local_time::local_date_time &ldt)
{
  try
  {
    if (!q.param(kFmiWindSpeedMS))
      return Spine::TimeSeries::None();

    float wspd = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (!q.param(kFmiTemperature))
      return Spine::TimeSeries::None();

    float t2m = q.info()->LandscapeInterpolatedValue(
        loc.dem, iswater(loc), NFmiPoint(loc.longitude, loc.latitude), ldt);

    if (wspd == kFloatMissing || t2m == kFloatMissing)
      return Spine::TimeSeries::None();

    float chill = FmiWindChill(wspd, t2m);
    return chill;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief SummerSimmerIndex
 */
// ----------------------------------------------------------------------

ts::Value SummerSimmerIndex(QImpl &q,
                            const Spine::Location &loc,
                            const boost::local_time::local_date_time &ldt)
{
  try
  {
    if (!q.param(kFmiHumidity))
      return Spine::TimeSeries::None();

    float rh = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (!q.param(kFmiTemperature))
      return Spine::TimeSeries::None();

    float t2m = q.info()->LandscapeInterpolatedValue(
        loc.dem, iswater(loc), NFmiPoint(loc.longitude, loc.latitude), ldt);

    if (rh == kFloatMissing || t2m == kFloatMissing)
      return Spine::TimeSeries::None();

    float ssi = FmiSummerSimmerIndex(rh, t2m);
    return ssi;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief FeelsLike temperature
 */
// ----------------------------------------------------------------------

ts::Value FeelsLike(QImpl &q,
                    const Spine::Location &loc,
                    const boost::local_time::local_date_time &ldt)
{
  try
  {
    if (!q.param(kFmiHumidity))
      return Spine::TimeSeries::None();

    float rh = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (!q.param(kFmiWindSpeedMS))
      return Spine::TimeSeries::None();

    float wspd = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (!q.param(kFmiTemperature))
      return Spine::TimeSeries::None();

    float t2m = q.info()->LandscapeInterpolatedValue(
        loc.dem, iswater(loc), NFmiPoint(loc.longitude, loc.latitude), ldt);

    if (rh == kFloatMissing || t2m == kFloatMissing || wspd == kFloatMissing)
      return Spine::TimeSeries::None();

    // We permit radiation to be missing
    float rad = kFloatMissing;
    if (q.param(kFmiRadiationGlobal))
      rad = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    float ret = FmiFeelsLikeTemperature(wspd, rh, t2m, rad);

    if (ret == kFloatMissing)
      return Spine::TimeSeries::None();
    else
      return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Apparent Temperature
 */
// ----------------------------------------------------------------------

ts::Value ApparentTemperature(QImpl &q,
                              const Spine::Location &loc,
                              const boost::local_time::local_date_time &ldt)
{
  try
  {
    if (!q.param(kFmiHumidity))
      return Spine::TimeSeries::None();

    float rh = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (!q.param(kFmiWindSpeedMS))
      return Spine::TimeSeries::None();

    float wspd = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (!q.param(kFmiTemperature))
      return Spine::TimeSeries::None();

    float t2m = q.info()->LandscapeInterpolatedValue(
        loc.dem, iswater(loc), NFmiPoint(loc.longitude, loc.latitude), ldt);

    if (rh == kFloatMissing || t2m == kFloatMissing || wspd == kFloatMissing)
      return Spine::TimeSeries::None();

    float ret = FmiApparentTemperature(wspd, rh, t2m);

    if (ret == kFloatMissing)
      return Spine::TimeSeries::None();
    else
      return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Lower limit of water to snow conversion
 */
// ----------------------------------------------------------------------
ts::Value Snow1hLower(QImpl &q,
                      const Spine::Location &loc,
                      const boost::local_time::local_date_time &ldt)
{
  try
  {
    if (!q.param(kFmiPrecipitation1h))
      return Spine::TimeSeries::None();

    float prec1h = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    // FmiSnowLowerLimit fails if input is 'nan', check here.

    if (prec1h == kFloatMissing)
    {
      return Spine::TimeSeries::None();
    }
    else
    {
      float ret = FmiSnowLowerLimit(prec1h);
      if (ret == kFloatMissing)
        return Spine::TimeSeries::None();
      else
        return ret;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Upper limit of water to snow conversion
 */
// ----------------------------------------------------------------------
ts::Value Snow1hUpper(QImpl &q,
                      const Spine::Location &loc,
                      const boost::local_time::local_date_time &ldt)
{
  try
  {
    if (!q.param(kFmiPrecipitation1h))
      return Spine::TimeSeries::None();

    float prec1h = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    // FmiSnowUpperLimit fails if input is 'nan', check here.
    if (prec1h == kFloatMissing)
    {
      return Spine::TimeSeries::None();
    }
    else
    {
      float ret = FmiSnowUpperLimit(prec1h);
      if (ret == kFloatMissing)
        return Spine::TimeSeries::None();
      else
        return ret;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Snow estimate if no Snow1h parameter present
 */
// ----------------------------------------------------------------------
ts::Value Snow1h(QImpl &q,
                 const Spine::Location &loc,
                 const boost::local_time::local_date_time &ldt)
{
  try
  {
    // Use the actual Snow1h if it is present
    if (q.param(kFmiSnow1h))
      return q.param(kFmiSnow1h);

    if (!q.param(kFmiTemperature))
      return Spine::TimeSeries::None();

    float t = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (!q.param(kFmiWindSpeedMS))
      return Spine::TimeSeries::None();

    float wspd = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (!q.param(kFmiPrecipitation1h))
      return Spine::TimeSeries::None();

    float prec1h = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (t == kFloatMissing || wspd == kFloatMissing || prec1h == kFloatMissing)
      return Spine::TimeSeries::None();

    float snow1h = prec1h * FmiSnowWaterRatio(t, wspd);  // Can this be kFLoatMissing???
    return snow1h;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WeatherSymbol = WeatherSymbol3 + 100*Dark
 */
// ----------------------------------------------------------------------

ts::Value WeatherSymbol(QImpl &q,
                        const Spine::Location &loc,
                        const boost::local_time::local_date_time &ldt)
{
  try
  {
    if (!q.param(kFmiWeatherSymbol3))
      return Spine::TimeSeries::None();

    float symbol = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    Fmi::Astronomy::solar_position_t sp =
        Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
    if (sp.dark())
      return 100 + symbol;
    else
      return symbol;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Weather text
 */
// ----------------------------------------------------------------------

ts::Value WeatherText(QImpl &q,
                      const Spine::Location &loc,
                      const boost::local_time::local_date_time &ldt,
                      const std::string &lang)
{
  try
  {
    if (!q.param(kFmiWeatherSymbol3))
      return Spine::TimeSeries::None();

    float w = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (w == kFloatMissing)
      return Spine::TimeSeries::None();

    // Source: /smartdev/www/wap.weatherproof.fi/sms/funcs.php
    if (lang == "en")
    {
      switch (boost::numeric_cast<int>(w))
      {
        case 1:
          return "sunny";
        case 2:
          return "partly cloudy";
        case 3:
          return "cloudy";
        case 21:
          return "light showers";
        case 22:
          return "showers";
        case 23:
          return "heavy showers";
        case 31:
          return "light rain";
        case 32:
          return "rain";
        case 33:
          return "heavy rain";
        case 41:
          return "light snow showers";
        case 42:
          return "snow showers";
        case 43:
          return "heavy snow showers";
        case 51:
          return "light snowfall";
        case 52:
          return "snowfall";
        case 53:
          return "heavy snowfall";
        case 61:
          return "thundershowers";
        case 62:
          return "heavy thundershowers";
        case 63:
          return "thunder";
        case 64:
          return "heavy thunder";
        case 71:
          return "light sleet showers";
        case 72:
          return "sleet showers";
        case 73:
          return "heavy sleet showers";
        case 81:
          return "light sleet rain";
        case 82:
          return "sleet rain";
        case 83:
          return "heavy sleet rain";
        case 91:
          return "fog";
        case 92:
          return "fog";
      }
    }
    else if (lang == "sv")
    {
      // From http://sv.ilmatieteenlaitos.fi/vadersymbolerna
      switch (boost::numeric_cast<int>(w))
      {
        case 1:
          return "klart";
        case 2:
          return "halvklart";
        case 3:
          return "mulet";
        case 21:
          return "lätta regnskurar";
        case 22:
          return "regnskurar";
        case 23:
          return "kraftiga regnskurar";
        case 31:
          return "lätt regn";
        case 32:
          return "regn";
        case 33:
          return "rikligt regn";
        case 41:
          return "lätta snöbyar";
        case 42:
          return "snöbyar";
        case 43:
          return "täta snöbyar";
        case 51:
          return "lätt snöfall";
        case 52:
          return "snöfall";
        case 53:
          return "ymnigt snöfall";
        case 61:
          return "åskskurar";
        case 62:
          return "kraftiga åskskurar";
        case 63:
          return "åska";
        case 64:
          return "häftigt åskväder";
        case 71:
          return "lätta skurar av snöblandat regn";
        case 72:
          return "skurar av snöblandat regn";
        case 73:
          return "kraftiga skurar av snöblandad regn";
        case 81:
          return "lätt snöblandat regn";
        case 82:
          return "snöblandat regn";
        case 83:
          return "kraftigt snöblandat regn";
        case 91:
          return "dis";
        case 92:
          return "dimma";
      }
    }
    else if (lang == "et")
    {
      switch (boost::numeric_cast<int>(w))
      {
        case 1:
          return "selge";
        case 2:
          return "poolpilves";
        case 3:
          return "pilves";
        case 21:
          return "kerged vihmahood";
        case 22:
          return "hoogvihm";
        case 23:
          return "tugevad vihmahood";
        case 31:
          return "nõrk vihmasadu";
        case 32:
          return "vihmasadu";
        case 33:
          return "vihmasadu";
        case 41:
          return "nõrgad lumehood";
        case 42:
          return "hooglumi";
        case 43:
          return "tihedad lumesajuhood";
        case 51:
          return "nõrk lumesadu";
        case 52:
          return "lumesadu";
        case 53:
          return "tihe lumesadu";
        case 61:
          return "äikesehood";
        case 62:
          return "tugevad äikesehood";
        case 63:
          return "äike";
        case 64:
          return "tugev äike";
        case 71:
          return "ñörgad lörtsihood";
        case 72:
          return "lörtsihood";
        case 73:
          return "tugev lörtsihood";
        case 81:
          return "nõrk lörtsisadu";
        case 82:
          return "lörtsisadu";
        case 83:
          return "tugev lörtsisadu";
        case 91:
          return "udu";
        case 92:
          return "uduvinet";
      }
    }
    else
    {
      switch (boost::numeric_cast<int>(w))
      {
        case 1:
          return "selkeää";
        case 2:
          return "puolipilvistä";
        case 3:
          return "pilvistä";
        case 21:
          return "heikkoja sadekuuroja";
        case 22:
          return "sadekuuroja";
        case 23:
          return "voimakkaita sadekuuroja";
        case 31:
          return "heikkoa vesisadetta";
        case 32:
          return "vesisadetta";
        case 33:
          return "voimakasta vesisadetta";
        case 41:
          return "heikkoja lumikuuroja";
        case 42:
          return "lumikuuroja";
        case 43:
          return "voimakkaita lumikuuroja";
        case 51:
          return "heikkoa lumisadetta";
        case 52:
          return "lumisadetta";
        case 53:
          return "voimakasta lumisadetta";
        case 61:
          return "ukkoskuuroja";
        case 62:
          return "voimakkaita ukkoskuuroja";
        case 63:
          return "ukkosta";
        case 64:
          return "voimakasta ukkosta";
        case 71:
          return "heikkoja räntäkuuroja";
        case 72:
          return "räntäkuuroja";
        case 73:
          return "voimakkaita räntäkuuroja";
        case 81:
          return "heikkoa räntäsadetta";
        case 82:
          return "räntäsadetta";
        case 83:
          return "voimakasta räntäsadetta";
        case 91:
          return "utua";
        case 92:
          return "sumua";
      }
    }

    return Spine::TimeSeries::None();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ======================================================================

ts::Value QImpl::value(ParameterOptions &opt, const boost::local_time::local_date_time &ldt)
{
  try
  {
    // Default return value
    ts::Value retval = Spine::TimeSeries::None();

    // Some shorthand variables
    const std::string &pname = opt.par.name();
    const Spine::Location &loc = opt.loc;

    // Update last accessed point.

    NFmiPoint latlon(loc.longitude, loc.latitude);

    switch (opt.par.type())
    {
      case Spine::Parameter::Type::Landscaped:
      {
        // We can landscape only surface data
        if (itsModels[0]->levelName() == "surface")
        {
          if (param(opt.par.number()))
          {
            time(ldt);

            bool iswater = Fmi::LandCover::isOpenWater(loc.covertype);
            // DEM data is more accurate
            if (loc.dem == 0)
              iswater = true;

            retval = itsInfo->LandscapeInterpolatedValue(loc.dem, iswater, latlon, ldt);
          }
          break;
        }
        // Fall through to normal handling if the data is not surface data,
      }
      case Spine::Parameter::Type::Data:
      {
        opt.lastpoint = latlon;

        if (param(opt.par.number()))
        {
          NFmiMetTime t = ldt;

          // Change the year if the data contains climatology
          if (isClimatology())
          {
            int year = originTime().PosixTime().date().year();
            t.SetYear(boost::numeric_cast<short>(year));
          }

          float interpolatedValue = interpolate(latlon, t, maxgap);

          // If we got no value and the proper flag is on,
          // find the nearest point with valid values and use
          // the values from that point

          if (interpolatedValue == kFloatMissing && opt.findnearestvalidpoint)
          {
            interpolatedValue = interpolate(opt.nearestpoint, t, maxgap);
            if (interpolatedValue != kFloatMissing)
              opt.lastpoint = opt.nearestpoint;
          }

          if (interpolatedValue == kFloatMissing)
            retval = Spine::TimeSeries::None();
          else
            retval = interpolatedValue;
        }

        break;
      }
      case Spine::Parameter::Type::DataDerived:
      {
        if (pname == "windcompass8")
          retval = WindCompass8(*this, loc, ldt);

        else if (pname == "windcompass16")
          retval = WindCompass16(*this, loc, ldt);

        else if (pname == "windcompass32")
          retval = WindCompass32(*this, loc, ldt);

        else if (pname == "cloudiness8th")
          retval = Cloudiness8th(*this, loc, ldt);

        else if (pname == "windchill")
          retval = WindChill(*this, loc, ldt);

        else if (pname == "summersimmerindex" || pname == "ssi")
          retval = SummerSimmerIndex(*this, loc, ldt);

        else if (pname == "feelslike")
          retval = FeelsLike(*this, loc, ldt);

        else if (pname == "apparenttemperature")
          retval = ApparentTemperature(*this, loc, ldt);

        else if (pname == "weather")
          retval = WeatherText(*this, loc, ldt, opt.language);

        else if (pname == "weathersymbol")
          retval = WeatherSymbol(*this, loc, ldt);

        else if (pname == "snow1hlower")
          retval = Snow1hLower(*this, loc, ldt);

        else if (pname == "snow1hupper")
          retval = Snow1hUpper(*this, loc, ldt);

        else if (pname == "snow1h")
          retval = Snow1h(*this, loc, ldt);

        else
          throw Spine::Exception(BCP, "Unknown DataDerived parameter '" + pname + "'!");

        break;
      }
      case Spine::Parameter::Type::DataIndependent:
      {
        const std::string &pname = opt.par.name();

        if (pname == "place")
          retval = opt.place;

        else if (pname == "name")
          retval = loc.name;

        else if (pname == "iso2")
          retval = loc.iso2;

        else if (pname == "geoid")
        {
          if (loc.geoid == 0)  // not sure why this is still here
            retval = Spine::TimeSeries::None();
          else
            retval = Fmi::to_string(loc.geoid);
        }

        else if (pname == "region")
        {
          // This reintroduces an older bug/feature where
          // the name of the location is given as a region
          // if it doesn't belong to any administrative region.
          // (i.e. Helsinki doesn't have region, Kumpula has.)
          //
          // Also checking whether the loc.name has valid data,
          // if it's empty as well - which shoudn't occur - we return nan

          if (loc.area.empty())
          {
            if (loc.name.empty())
            {
              // No area (administrative region) nor name known.
              retval = Spine::TimeSeries::None();
            }
            else
            {
              // Place name known, administrative region unknown.
              retval = loc.name;
            }
          }
          else
          {
            // Administrative region known.
            retval = loc.area;
          }
        }

        else if (pname == "country")
          retval = opt.country;

        else if (pname == "feature")
          retval = loc.feature;

        else if (pname == "tz")
        {
          if (ldt.zone())
            retval = ldt.zone()->std_zone_name();
          else
            retval = Spine::TimeSeries::None();
        }

        else if (pname == "localtz")
          retval = loc.timezone;

        else if (pname == "level")
          retval = Fmi::to_string(levelValue());

        else if (pname == "latlon" || pname == "lonlat")
          retval = ts::LonLat(loc.longitude, loc.latitude);

        else if (pname == "nearlatitude")
          retval = opt.lastpoint.Y();

        else if (pname == "nearlongitude")
          retval = opt.lastpoint.X();

        else if (pname == "nearlatlon" || pname == "nearlonlat")
          retval = ts::LonLat(opt.lastpoint.X(), opt.lastpoint.Y());

        else if (pname == "population")
          retval = Fmi::to_string(loc.population);

        else if (pname == "elevation")
          retval = Fmi::to_string(loc.elevation);

        else if (pname == "dem")
          retval = Fmi::to_string(loc.dem);

        else if (pname == "covertype")
          retval = Fmi::to_string(static_cast<int>(loc.covertype));

        else if (pname == "model")
          retval = opt.producer;

        else if (pname == "time")
          retval = opt.timeformatter.format(ldt);

        else if (pname == "isotime")
          retval = Fmi::to_iso_string(ldt.local_time());

        else if (pname == "xmltime")
          retval = Fmi::to_iso_extended_string(ldt.local_time());

        else if (pname == "localtime")
        {
          boost::local_time::time_zone_ptr localtz =
              Fmi::TimeZoneFactory::instance().time_zone_from_string(loc.timezone);

          boost::posix_time::ptime utc = ldt.utc_time();
          boost::local_time::local_date_time localt(utc, localtz);
          retval = opt.timeformatter.format(localt);
        }

        else if (pname == "utctime")
          retval = opt.timeformatter.format(ldt.utc_time());

        else if (pname == "epochtime")
        {
          boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
          boost::posix_time::time_duration diff = ldt.utc_time() - time_t_epoch;
          retval = Fmi::to_string(diff.total_seconds());
        }

        else if (pname == "origintime")
        {
          boost::posix_time::ptime utc = originTime();
          boost::local_time::local_date_time localt(utc, ldt.zone());
          retval = opt.timeformatter.format(localt);
        }

        else if (pname == "modtime" || pname == "mtime")
        {
          boost::posix_time::ptime utc = modificationTime();
          boost::local_time::local_date_time localt(utc, ldt.zone());
          retval = opt.timeformatter.format(localt);
        }

        else if (pname == "dark")
        {
          Fmi::Astronomy::solar_position_t sp =
              Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_string(static_cast<int>(sp.dark()));
        }

        else if (pname == "moonphase")
          retval = Fmi::Astronomy::moonphase(ldt.utc_time());

        else if (pname == "moonrise")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_iso_string(lt.moonrise.local_time());
        }
        else if (pname == "moonrise2")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

          if (lt.moonrise2_today())
            retval = Fmi::to_iso_string(lt.moonrise2.local_time());
          else
            retval = std::string("");
        }
        else if (pname == "moonset")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_iso_string(lt.moonset.local_time());
        }
        else if (pname == "moonset2")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

          if (lt.moonset2_today())
          {
            retval = Fmi::to_iso_string(lt.moonset2.local_time());
          }
          else
          {
            retval = std::string("");
          }
        }
        else if (pname == "moonrisetoday")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

          retval = Fmi::to_string(static_cast<int>(lt.moonrise_today()));
        }
        else if (pname == "moonrise2today")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_string(static_cast<int>(lt.moonrise2_today()));
        }
        else if (pname == "moonsettoday")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_string(static_cast<int>(lt.moonset_today()));
        }
        else if (pname == "moonset2today")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_string(static_cast<int>(lt.moonset2_today()));
        }
        else if (pname == "moonup24h")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_string(static_cast<int>(lt.above_horizont_24h()));
        }
        else if (pname == "moondown24h")
        {
          Fmi::Astronomy::lunar_time_t lt =
              Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_string(static_cast<int>(!lt.moonrise_today() && !lt.moonset_today() &&
                                                   !lt.above_horizont_24h()));
        }

        else if (pname == "sunrise")
        {
          Fmi::Astronomy::solar_time_t st =
              Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_iso_string(st.sunrise.local_time());
        }
        else if (pname == "sunset")
        {
          Fmi::Astronomy::solar_time_t st =
              Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_iso_string(st.sunset.local_time());
        }
        else if (pname == "noon")
        {
          Fmi::Astronomy::solar_time_t st =
              Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_iso_string(st.noon.local_time());
        }
        else if (pname == "sunrisetoday")
        {
          Fmi::Astronomy::solar_time_t st =
              Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_string(static_cast<int>(st.sunrise_today()));
        }
        else if (pname == "sunsettoday")
        {
          Fmi::Astronomy::solar_time_t st =
              Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
          retval = Fmi::to_string(static_cast<int>(st.sunset_today()));
        }
        else if (pname == "daylength")
        {
          Fmi::Astronomy::solar_time_t st =
              Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
          auto seconds = st.daylength().total_seconds();
          auto minutes = boost::numeric_cast<long>(round(seconds / 60.0));
          retval = Fmi::to_string(minutes);
        }
        else if (pname == "timestring")
          retval = format_date(ldt, opt.outlocale, opt.timestring.c_str());

        else if (pname == "wday")
          retval = format_date(ldt, opt.outlocale, "%a");

        else if (pname == "weekday")
          retval = format_date(ldt, opt.outlocale, "%A");

        else if (pname == "mon")
          retval = format_date(ldt, opt.outlocale, "%b");

        else if (pname == "month")
          retval = format_date(ldt, opt.outlocale, "%B");

        else if (pname == "hour")
          retval = Fmi::to_string(ldt.local_time().time_of_day().hours());

        else if (pname.substr(0, 5) == "date(" && pname[pname.size() - 1] == ')')
          retval = format_date(ldt, opt.outlocale, pname.substr(5, pname.size() - 6));

        else if (pname == "latitude" || pname == "lat")
          retval = loc.latitude;

        else if (pname == "longitude" || pname == "lon")
          retval = loc.longitude;

        else if (pname == "sunelevation")
        {
          Fmi::Astronomy::solar_position_t sp =
              Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
          retval = sp.elevation;
        }

        else if (pname == "sundeclination")
        {
          Fmi::Astronomy::solar_position_t sp =
              Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
          retval = sp.declination;
        }

        else if (pname == "sunazimuth")
        {
          Fmi::Astronomy::solar_position_t sp =
              Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
          retval = sp.azimuth;
        }

        // The following parameters are added for for obsengine compability reasons
        // so that we can have e.g. fmisid identifier for observations in query which
        // has both observations and forecasts
        else if (pname == "fmisid" || pname == "wmo" || pname == "lpnn" || pname == "rwsid" ||
                 pname == "stationary" || pname == "distance" || pname == "direction" ||
                 pname == "sensor_no" || pname == "stationname")
          retval = Spine::TimeSeries::None();

        else
          throw Spine::Exception(BCP, "Unknown DataIndependent special function '" + pname + "'!");
      }
    }

    if (boost::get<double>(&retval))
    {
      if (*(boost::get<double>(&retval)) == kFloatMissing)
        retval = Spine::TimeSeries::None();
    }

    return retval;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// one location, many timesteps
ts::TimeSeriesPtr QImpl::values(ParameterOptions &param,
                                const Spine::TimeSeriesGenerator::LocalTimeList &tlist)
{
  try
  {
    ts::TimeSeriesPtr ret(new ts::TimeSeries);

    BOOST_FOREACH (const boost::local_time::local_date_time &ldt, tlist)
    {
      ret->push_back(ts::TimedValue(ldt, value(param, ldt)));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// many locations (indexmask), many timesteps
ts::TimeSeriesGroupPtr QImpl::values(ParameterOptions &param,
                                     const NFmiIndexMask &indexmask,
                                     const Spine::TimeSeriesGenerator::LocalTimeList &tlist)
{
  try
  {
    ts::TimeSeriesGroupPtr ret(new ts::TimeSeriesGroup);

    for (NFmiIndexMask::const_iterator it = indexmask.begin(); it != indexmask.end(); ++it)
    {
      // Indexed latlon
      NFmiPoint latlon(latLon(*it));

      Spine::Location location(param.loc.geoid,
                               param.loc.name,
                               param.loc.iso2,
                               param.loc.municipality,
                               param.loc.area,
                               param.loc.feature,
                               param.loc.country,
                               latlon.X(),
                               latlon.Y(),
                               param.loc.timezone,
                               param.loc.population,
                               param.loc.elevation,
                               param.loc.priority);

      ParameterOptions paramOptions(param.par,
                                    param.producer,
                                    location,
                                    param.country,
                                    param.place,
                                    param.timeformatter,
                                    param.timestring,
                                    param.language,
                                    param.outlocale,
                                    param.outzone,
                                    param.findnearestvalidpoint,
                                    param.nearestpoint,
                                    param.lastpoint);

      ts::TimeSeriesPtr timeseries = values(paramOptions, tlist);
      ts::LonLat lonlat(latlon.X(), latlon.Y());

      ret->push_back(ts::LonLatTimeSeries(lonlat, *timeseries));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// many locations (llist), many timesteps

// BUG?? Why is maxdistance in the API?

ts::TimeSeriesGroupPtr QImpl::values(ParameterOptions &param,
                                     const Spine::LocationList &llist,
                                     const Spine::TimeSeriesGenerator::LocalTimeList &tlist,
                                     const double & /* maxdistance */)
{
  try
  {
    ts::TimeSeriesGroupPtr ret(new ts::TimeSeriesGroup);

    BOOST_FOREACH (const Spine::LocationPtr &loc, llist)
    {
      ParameterOptions paramOptions(param.par,
                                    param.producer,
                                    *loc,
                                    param.country,
                                    param.place,
                                    param.timeformatter,
                                    param.timestring,
                                    param.language,
                                    param.outlocale,
                                    param.outzone,
                                    param.findnearestvalidpoint,
                                    param.nearestpoint,
                                    param.lastpoint);

      ts::TimeSeriesPtr timeseries = values(paramOptions, tlist);
      ts::LonLat lonlat(loc->longitude, loc->latitude);

      ret->push_back(ts::LonLatTimeSeries(lonlat, *timeseries));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Load dem values and water flags for native (sub)grid or for given locations.
 * 		  Returns false if there's no valid location (within the native grid)
 */
// ----------------------------------------------------------------------

bool QImpl::loadDEMAndWaterFlags(const Fmi::DEM &theDem,
                                 const Fmi::LandCover &theLandCover,
                                 double theResolution,
                                 const NFmiDataMatrix<NFmiLocationCache> &theLocationCache,
                                 NFmiDataMatrix<float> &theDemMatrix,
                                 NFmiDataMatrix<bool> &theWaterFlagMatrix,
                                 int x1,
                                 int y1,
                                 int x2,
                                 int y2) const
{
  try
  {
    if (!isGrid())
      throw Spine::Exception(BCP, "Can only be used for gridded data!");

    // Resolution must be given with locations

    if (theResolution < 0)
      throw Spine::Exception(BCP, "Resolution must be nonnegative!");
    else if (theResolution < 0.01)
    {
      if (theResolution > 0)
      {
        throw Spine::Exception(BCP, "Resolutions below 10 meters are not supported!");
      }
      else if (theLocationCache.NX() > 0)
      {
        throw Spine::Exception(BCP, "Nonzero resolution must be given with locations!");
      }
    }

    const NFmiGrid &nativeGrid = grid();

    if (theLocationCache.NX() > 0)
    {
      // Load dem values and waterflags for given locations (target grid)
      //
      bool intersectsGrid = false;
      int nx = theLocationCache.NX(), ny = theLocationCache.NY();

      theDemMatrix.Resize(nx, ny);
      theWaterFlagMatrix.Resize(nx, ny);

      for (int i = 0; (i < nx); i++)
        for (int j = 0; (j < ny); j++)
        {
          auto const &loc = theLocationCache[i][j];

          if (loc.itsLocationIndex != static_cast<unsigned long>(-1))
          {
            auto latLon = nativeGrid.GridToLatLon(loc.itsGridPoint);
            auto dem = theDem.elevation(latLon.X(), latLon.Y(), theResolution);

            theDemMatrix[i][j] = dem;
            theWaterFlagMatrix[i][j] =
                ((dem == 0) ||
                 Fmi::LandCover::isOpenWater(theLandCover.coverType(latLon.X(), latLon.Y())));

            intersectsGrid = true;
          }
          else
          {
            theDemMatrix[i][j] = kFloatMissing;
            theWaterFlagMatrix[i][j] = false;
          }
        }

      return intersectsGrid;
    }

    // Load dem values and waterflags for native grid points.
    //
    // When cropping, extend the subgrid dimensions by 1 if possible to be get values for the last
    // column and row
    // (landscaping requires neighbour gridpoints to be available)

    int nativeGridSizeX = nativeGrid.XNumber(), nativeGridSizeY = nativeGrid.YNumber(), nx, ny;

    if ((x1 != 0) || (y1 != 0) || (x2 != 0) || (y2 != 0))
    {
      if (!((x1 >= 0) && (x1 < x2) && (y1 >= 0) && (y1 < y2) && (x2 < nativeGridSizeX) &&
            (y2 < nativeGridSizeY)))
        throw Spine::Exception(BCP, "Cropping is invalid or outside the grid!");

      if (x2 < (nativeGridSizeX - 1))
        x2++;
      if (y2 < (nativeGridSizeY - 1))
        y2++;

      nx = x2 - x1 + 1;
      ny = y2 - y1 + 1;
    }
    else
    {
      nx = nativeGridSizeX;
      x1 = 0, x2 = nativeGridSizeX - 1;
      ny = nativeGridSizeY;
      y1 = 0;
      y2 = nativeGridSizeY - 1;
    }

    // Default resolution is the grid resolution

    if (theResolution == 0)
      theResolution = (nativeGrid.Area()->WorldXYWidth() / nativeGridSizeX) / 1000;

    theDemMatrix.Resize(nx, ny);
    theWaterFlagMatrix.Resize(nx, ny);

    for (int i = x1, i0 = 0; (i <= x2); i++, i0++)
      for (int j = y1, j0 = 0; (j <= y2); j++, j0++)
      {
        auto latLon = nativeGrid.GridToLatLon(i, j);
        auto dem = theDem.elevation(latLon.X(), latLon.Y(), theResolution);

        theDemMatrix[i0][j0] = dem;
        theWaterFlagMatrix[i0][j0] =
            ((dem == 0) ||
             Fmi::LandCover::isOpenWater(theLandCover.coverType(latLon.X(), latLon.Y())));
      }

    return true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Sample the data to create a new Q object
 */
// ----------------------------------------------------------------------

Q QImpl::sample(const Spine::Parameter &theParameter,
                const boost::posix_time::ptime &theTime,
                const OGRSpatialReference &theCrs,
                double theXmin,
                double theYmin,
                double theXmax,
                double theYmax,
                double theResolution,
                const Fmi::DEM &theDem,
                const Fmi::LandCover &theLandCover)
{
  try
  {
    if (!param(theParameter.number()))
      throw Spine::Exception(
          BCP,
          "Parameter " + theParameter.name() + " is not available for sampling in the querydata");

    if (theResolution <= 0)
      throw Spine::Exception(BCP, "The sampling resolution must be nonnegative");

    if (theResolution < 0.01)
      throw Spine::Exception(BCP, "Sampling resolutions below 10 meters are not supported");

    if (!itsInfo->TimeDescriptor().IsInside(theTime))
      throw Spine::Exception(BCP, "Cannot sample data to a time outside the querydata");

    if (!itsInfo->IsGrid())
      throw Spine::Exception(BCP, "Cannot sample point data to new resolution");

    // Establish the new descriptors

    NFmiVPlaceDescriptor vdesc(itsInfo->VPlaceDescriptor());

    NFmiParamBag pbag;
    pbag.Add(itsInfo->Param());
    NFmiParamDescriptor pdesc(pbag);

    NFmiTimeList tlist;
    tlist.Add(new NFmiMetTime(theTime));
    NFmiTimeDescriptor tdesc(itsInfo->OriginTime(), tlist);

    // Establish new projection and the required grid size of the desired resolution

    auto newarea =
        boost::make_shared<NFmiGdalArea>("FMI", theCrs, theXmin, theYmin, theXmax, theYmax);
    double datawidth = newarea->WorldXYWidth() / 1000.0;  // view extent in kilometers
    double dataheight = newarea->WorldXYHeight() / 1000.0;
    int width = static_cast<int>(datawidth / theResolution);
    int height = static_cast<int>(dataheight / theResolution);

    // Must use at least two grid points, value 1 would cause a segmentation fault in here
    width = std::max(width, 2);
    height = std::max(height, 2);

    NFmiGrid grid(newarea.get(), width, height);
    NFmiHPlaceDescriptor hdesc(grid);

    // Then create the new querydata

    NFmiFastQueryInfo info(pdesc, tdesc, hdesc, vdesc);
    boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
    if (data.get() == 0)
      throw Spine::Exception(BCP, "Failed to create querydata by sampling");

    NFmiFastQueryInfo dstinfo(data.get());
    dstinfo.First();  // sets the only param and time active

    if ((itsModels[0]->levelName() == "surface") &&
        (theParameter.type() == Spine::Parameter::Type::Landscaped))
    {
      // Landscaping; temperature or dewpoint
      //
      NFmiDataMatrix<NFmiLocationCache> locCache;
      NFmiTimeCache timeCache = itsInfo->CalcTimeCache(NFmiMetTime(theTime));
      NFmiDataMatrix<float> valueMatrix, demMatrix;
      NFmiDataMatrix<bool> waterFlagMatrix;

      calcLatlonCachePoints(dstinfo, locCache);

      if (loadDEMAndWaterFlags(
              theDem, theLandCover, theResolution, locCache, demMatrix, waterFlagMatrix))
        landscapeCachedInterpolation(valueMatrix, locCache, timeCache, demMatrix, waterFlagMatrix);
      else
        // Target grid does not intersect the native grid
        //
        valueMatrix.Resize(locCache.NX(), locCache.NY(), kFloatMissing);

      int nx = valueMatrix.NX(), n;

      for (dstinfo.ResetLocation(), n = 0; dstinfo.NextLocation(); n++)
      {
        int i = n % nx;
        int j = n / nx;

        dstinfo.FloatValue(valueMatrix[i][j]);
      }
    }
    else
    {
      // Now we need all kinds of extra variables because of the damned API

      NFmiPoint dummy;
      boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
      boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
      boost::local_time::local_date_time localdatetime(theTime, utc);

      for (dstinfo.ResetLevel(); dstinfo.NextLevel();)
      {
        itsInfo->Level(*dstinfo.Level());
        for (dstinfo.ResetLocation(); dstinfo.NextLocation();)
        {
          auto latlon = dstinfo.LatLon();
          auto dem = theDem.elevation(latlon.X(), latlon.Y(), theResolution);
          auto covertype = theLandCover.coverType(latlon.X(), latlon.Y());
          Spine::Location loc(latlon.X(), latlon.Y(), dem, covertype);
          // Paska API...
          ParameterOptions options(theParameter,
                                   Producer(),
                                   loc,
                                   "",
                                   "",
                                   *timeformatter,
                                   "",
                                   "",
                                   std::locale::classic(),
                                   "",
                                   false,
                                   NFmiPoint(),
                                   dummy);

          auto result = value(options, localdatetime);
          if (boost::get<double>(&result))
            dstinfo.FloatValue(*boost::get<double>(&result));
        }
      }
    }

    // Return the new Q but with a new hash value

    std::size_t hash = itsHashValue;
    boost::hash_combine(hash, theResolution);
    boost::hash_combine(hash, to_simple_string(theTime));
    boost::hash_combine(hash, theXmin);
    boost::hash_combine(hash, theYmin);
    boost::hash_combine(hash, theXmax);
    boost::hash_combine(hash, theYmax);

    char *tmp;
    theCrs.exportToWkt(&tmp);
    boost::hash_combine(hash, tmp);
    OGRFree(tmp);

    auto model = boost::make_shared<Model>(*itsModels[0], data, hash);
    return boost::make_shared<QImpl>(model);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool QImpl::selectLevel(double theLevel)
{
  try
  {
    this->resetLevel();
    while (this->nextLevel())
    {
      if (this->levelValue() == theLevel)
        return true;
    }
    return false;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Retrun the grid hash value
 *
 * Note: All models are required to have the same grid
 */
// ----------------------------------------------------------------------

std::size_t QImpl::gridHashValue() const
{
  return itsModels.front()->gridHashValue();
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate a hash for the object
 */
// ----------------------------------------------------------------------

std::size_t hash_value(const QImpl &theQ)
{
  try
  {
    return theQ.itsHashValue;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet
