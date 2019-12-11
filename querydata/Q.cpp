#include "Q.h"
#include "Model.h"
#include <boost/date_time/local_time/local_time_io.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/time_facet.hpp>
#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/timer/timer.hpp>
#include <gis/Box.h>
#include <gis/DEM.h>
#include <gis/LandCover.h>
#include <macgyver/Astronomy.h>
#include <macgyver/CharsetTools.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TimeZoneFactory.h>
#include <newbase/NFmiMetMath.h>
#include <newbase/NFmiMultiQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeList.h>
#include <spine/Exception.h>
#include <spine/ParameterFactory.h>
#include <cassert>
#include <ogr_spatialref.h>
#include <stdexcept>

namespace ts = SmartMet::Spine::TimeSeries;

namespace
{
// SmartSymbol / WeatherNumber calculation limits

const float thunder_limit1 = 30;
const float thunder_limit2 = 60;

const float rain_limit1 = 0.025;
const float rain_limit2 = 0.04;
const float rain_limit3 = 0.4;
const float rain_limit4 = 1.5;
const float rain_limit5 = 2;
const float rain_limit6 = 4;
const float rain_limit7 = 7;

const int cloud_limit1 = 7;
const int cloud_limit2 = 20;
const int cloud_limit3 = 33;
const int cloud_limit4 = 46;
const int cloud_limit5 = 59;
const int cloud_limit6 = 72;
const int cloud_limit7 = 85;
const int cloud_limit8 = 93;

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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The destructor releases the NFmiFastQueryInfo back into a pool
 */
// ----------------------------------------------------------------------

QImpl::~QImpl()
{
  for (std::size_t i = 0; i < itsInfos.size(); i++)
    itsModels[i]->release(itsInfos[i]);
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
    itsModels.emplace_back(theModel);
    itsInfos.push_back(theModel->info());
    itsInfo = theModel->info();

    itsValidTimes = theModel->validTimes();

    itsHashValue = hash_value(theModel);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Construct from multiple models
 */
// ----------------------------------------------------------------------

QImpl::QImpl(const std::vector<SharedModel> &theModels)
    : itsModels(theModels), itsValidTimes(new ValidTimeList)
{
  try
  {
    if (theModels.empty())
      throw Spine::Exception(BCP, "Cannot initialize any empty view over multiple models");

    for (auto &model : itsModels)
      itsInfos.push_back(model->info());

    if (itsInfos.size() > 1)
      itsInfo = boost::make_shared<NFmiMultiQueryInfo>(itsInfos);
    else
      itsInfo = itsInfos[0];

    // Establish hash value
    itsHashValue = 0;
    for (const auto &model : itsModels)
    {
      boost::hash_combine(itsHashValue, model);
    }

    // Establish unique valid times
    std::set<boost::posix_time::ptime> uniquetimes;
    for (auto &model : itsModels)
    {
      const auto &validtimes = model->validTimes();
      for (const auto &t : *validtimes)
        uniquetimes.insert(t);
    }
    for (const auto &t : uniquetimes)
      itsValidTimes->push_back(t);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    // TODO(mheiskan): should not access NFmiFastQueryInfo directly
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
      auto t1 = qi.ValidTime();
      qi.NextTime();
      auto t2 = qi.ValidTime();
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
      auto dot = paramPrec.find('.');
      auto fchar = paramPrec.find('f');
      if ((dot != std::string::npos) && (fchar != std::string::npos))
      {
        auto theNumber = std::string(paramPrec.begin() + dot + 1, paramPrec.begin() + fchar);
        params.emplace_back(paramName, paramDesc, std::strtol(theNumber.c_str(), nullptr, 10));
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
    if (qi.Area() == nullptr)
    {
      meta.WKT = "nan";
      return meta;
    }

    meta.WKT = qi.Area()->WKT();

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const WGS84Envelope &QImpl::getWGS84Envelope()
{
  Spine::UpgradeReadLock readlock(itsWGS84EnvelopeMutex);

  if (itsWGS84Envelope != nullptr)
    return *itsWGS84Envelope;

  Spine::UpgradeWriteLock writelock(readlock);

  if (itsWGS84Envelope == nullptr)
    itsWGS84Envelope = boost::movelib::make_unique<WGS84Envelope>(itsInfo);
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      t = std::max(t, itsModels[i]->modificationTime());

    return t;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return estimated expiration time of the model
 */
// ----------------------------------------------------------------------

boost::posix_time::ptime QImpl::expirationTime() const
{
  try
  {
    auto t = itsModels[0]->expirationTime();

    for (std::size_t i = 1; i < itsModels.size(); i++)
      t = std::max(t, itsModels[i]->expirationTime());

    return t;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the wind U/V components are relative to the grid
 */
// ----------------------------------------------------------------------

bool QImpl::isRelativeUV() const
{
  try
  {
    return itsModels[0]->isRelativeUV();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    return (itsInfo->Area() != nullptr);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Reset the location iterator
 */
// ----------------------------------------------------------------------

void QImpl::resetLocation()
{
  itsInfo->ResetLocation();
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the first location
 */
// ----------------------------------------------------------------------

bool QImpl::firstLocation()
{
  return itsInfo->FirstLocation();
}

// ----------------------------------------------------------------------
/*!
 * \brief Advance the location iterator
 */
// ----------------------------------------------------------------------

bool QImpl::nextLocation()
{
  return itsInfo->NextLocation();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the current WorldXY coordinate
 */
// ----------------------------------------------------------------------
NFmiPoint QImpl::worldXY() const
{
  return itsInfo->WorldXY();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the current LatLon coordinate
 */
// ----------------------------------------------------------------------

NFmiPoint QImpl::latLon() const
{
  return itsInfo->LatLon();
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
    return (itsInfo->Grid() != nullptr);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    if (itsInfo->Area() == nullptr)
      throw Spine::Exception(BCP, "Attempt to access unset area in querydata");
    return *itsInfo->Area();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    if (itsInfo->Grid() == nullptr)
      throw Spine::Exception(BCP, "Attempt to access unset grid in querydata");
    return *itsInfo->Grid();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

float QImpl::interpolateAtPressure(const NFmiPoint &theLatLon,
                                   const NFmiMetTime &theTime,
                                   float thePressure,
                                   int theMaxMinuteGap)
{
  try
  {
    return itsInfo->PressureLevelValue(thePressure, theLatLon, theTime, theMaxMinuteGap);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

float QImpl::interpolateAtHeight(const NFmiPoint &theLatLon,
                                 const NFmiMetTime &theTime,
                                 float theHeight,
                                 int theMaxMinuteGap)
{
  try
  {
    return itsInfo->HeightValue(theHeight, theLatLon, theTime, theMaxMinuteGap);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Prepare cache values for speeding up time interpolation
 */
// ----------------------------------------------------------------------

bool QImpl::calcTimeCache(NFmiQueryInfo &theTargetInfo, std::vector<NFmiTimeCache> &theTimeCache)
{
  try
  {
    return itsInfo->CalcTimeCache(theTargetInfo, theTimeCache);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values at grid points
 * \param theDemValues DEM values for landscaping (an empty matrix by default)
 * \param theWaterFlags Water flags for landscaping (an empty matrix by default)
 */
// ----------------------------------------------------------------------

void QImpl::values(NFmiDataMatrix<float> &theMatrix,
                   const NFmiDataMatrix<float> &theDEMValues,
                   const NFmiDataMatrix<bool> &theWaterFlags)
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract time interpolated values at grid points
 * \param theInterpolatedTime The desired time
 * \param theDemValues DEM values for landscaping (an empty matrix by default)
 * \param theWaterFlags Water flags for landscaping (an empty matrix by default)
 */
// ----------------------------------------------------------------------

void QImpl::values(NFmiDataMatrix<float> &theMatrix,
                   const NFmiMetTime &theInterpolatedTime,
                   const NFmiDataMatrix<float> &theDEMValues,
                   const NFmiDataMatrix<bool> &theWaterFlags)
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract time interpolated values at grid points
 * \param theInterpolatedTime The desired time
 * \param theDemValues DEM values for landscaping (an empty matrix by default)
 * \param theWaterFlags Water flags for landscaping (an empty matrix by default)
 */
// ----------------------------------------------------------------------

void QImpl::values(NFmiDataMatrix<float> &theMatrix,
                   const Spine::Parameter &theParam,
                   const boost::posix_time::ptime &theInterpolatedTime,
                   const NFmiDataMatrix<float> &theDEMValues,
                   const NFmiDataMatrix<bool> &theWaterFlags)
{
  try
  {
    switch (theParam.type())
    {
      case Spine::Parameter::Type::Data:
      case Spine::Parameter::Type::Landscaped:
      {
        if (!param(theParam.number()))
          throw Spine::Exception(BCP,
                                 "Parameter " + theParam.name() + " is not available in the data");
        return values(theMatrix, theInterpolatedTime, theDEMValues, theWaterFlags);
      }
      case Spine::Parameter::Type::DataDerived:
      case Spine::Parameter::Type::DataIndependent:
      {
        const auto nx = grid().XNumber();
        const auto ny = grid().YNumber();
        theMatrix.Resize(nx, ny, kFloatMissing);

        // Now we need all kinds of extra variables because of the damned API

        NFmiPoint dummy;
        boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
        boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
        boost::local_time::local_date_time localdatetime(theInterpolatedTime, utc);

        auto mylocale = std::locale::classic();

        // we need to modify the coordinate for each point
        for (std::size_t j = 0; j < ny; j++)
          for (std::size_t i = 0; i < nx; i++)
          {
            auto coord = latLon(j * nx + i);
            Spine::Location loc(coord.X(), coord.Y());
            ParameterOptions opts(theParam,
                                  Producer(),
                                  loc,
                                  "",
                                  "",
                                  *timeformatter,
                                  "",
                                  "",
                                  mylocale,
                                  "",
                                  false,
                                  NFmiPoint(),
                                  dummy);
            auto result = value(opts, localdatetime);

            if (boost::get<double>(&result) != nullptr)
              theMatrix[i][j] = *boost::get<double>(&result);
          }
        break;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void QImpl::pressureValues(NFmiDataMatrix<float> &theValues,
                           const NFmiGrid &theWantedGrid,
                           const NFmiMetTime &theInterpolatedTime,
                           float wantedPressureLevel,
                           bool relative_uv)
{
  try
  {
    return itsInfo->PressureValues(
        theValues, theWantedGrid, theInterpolatedTime, wantedPressureLevel, relative_uv);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values for grid
 */
// ----------------------------------------------------------------------

void QImpl::gridValues(NFmiDataMatrix<float> &theValues,
                       const NFmiGrid &theWantedGrid,
                       const NFmiMetTime &theInterpolatedTime,
                       bool relative_uv)
{
  try
  {
    return itsInfo->GridValues(theValues, theWantedGrid, theInterpolatedTime, relative_uv);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values for height level
 */
// ----------------------------------------------------------------------

void QImpl::heightValues(NFmiDataMatrix<float> &theValues,
                         const NFmiGrid &theWantedGrid,
                         const NFmiMetTime &theInterpolatedTime,
                         float wantedHeightLevel,
                         bool relative_uv)
{
  try
  {
    return itsInfo->HeightValues(
        theValues, theWantedGrid, theInterpolatedTime, wantedHeightLevel, relative_uv);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    using tfacet = boost::date_time::time_facet<boost::local_time::local_date_time, char>;
    std::ostringstream os;
    os.imbue(std::locale(llocale, new tfacet(fmt.c_str())));
    os << ldt;
    return Fmi::latin1_to_utf8(os.str());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindUMS with true north orientation
 */
// ----------------------------------------------------------------------

ts::Value WindUMS(QImpl &q,
                  const Spine::Location &loc,
                  const boost::local_time::local_date_time &ldt)
{
  try
  {
    NFmiPoint latlon(loc.longitude, loc.latitude);
    auto angle = q.area().TrueNorthAzimuth(latlon).ToRad();

    if (!q.param(kFmiWindUMS))
      return Spine::TimeSeries::None();

    auto u = q.interpolate(latlon, ldt, maxgap);

    if (angle == 0)
      return u;

    if (!q.param(kFmiWindVMS))
      return Spine::TimeSeries::None();

    auto v = q.interpolate(latlon, ldt, maxgap);

    if (u == kFloatMissing || v == kFloatMissing)
      return Spine::TimeSeries::None();

    // Unrotate U by the given angle

    return u * cos(-angle) + v * sin(-angle);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindVMS with true north orientation
 */
// ----------------------------------------------------------------------

ts::Value WindVMS(QImpl &q,
                  const Spine::Location &loc,
                  const boost::local_time::local_date_time &ldt)
{
  try
  {
    NFmiPoint latlon(loc.longitude, loc.latitude);
    auto angle = q.area().TrueNorthAzimuth(latlon).ToRad();

    if (!q.param(kFmiWindVMS))
      return Spine::TimeSeries::None();

    auto v = q.interpolate(latlon, ldt, maxgap);

    if (angle == 0)
      return v;

    if (!q.param(kFmiWindUMS))
      return Spine::TimeSeries::None();

    auto u = q.interpolate(latlon, ldt, maxgap);

    if (u == kFloatMissing || v == kFloatMissing)
      return Spine::TimeSeries::None();

    // Unrotate V by the given angle

    return v * cos(-angle) - u * sin(-angle);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    static std::vector<std::string> names{"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

    if (!q.param(kFmiWindDirection))
      return Spine::TimeSeries::None();

    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (value == kFloatMissing)
      return Spine::TimeSeries::None();

    int i = static_cast<int>((value + 22.5) / 45) % 8;
    return names.at(i);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    static std::vector<std::string> names{"N",
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
    return names.at(i);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    static std::vector<std::string> names{"N", "NbE", "NNE", "NEbN", "NE", "NEbE", "ENE", "EbN",
                                          "E", "EbS", "ESE", "SEbE", "SE", "SEbS", "SSE", "SbE",
                                          "S", "SbW", "SSW", "SWbS", "SW", "SWbW", "WSW", "WbS",
                                          "W", "WbN", "WNW", "NWbW", "NW", "NWbN", "NNW", "NbW"};

    if (!q.param(kFmiWindDirection))
      return Spine::TimeSeries::None();

    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), ldt, maxgap);

    if (value == kFloatMissing)
      return Spine::TimeSeries::None();

    int i = static_cast<int>((value + 5.625) / 11.25) % 32;
    return names.at(i);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    float ret = FmiSnowLowerLimit(prec1h);
    if (ret == kFloatMissing)
      return Spine::TimeSeries::None();
    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
      return Spine::TimeSeries::None();

    float ret = FmiSnowUpperLimit(prec1h);
    if (ret == kFloatMissing)
      return Spine::TimeSeries::None();
    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    if (symbol == kFloatMissing)
      return kFloatMissing;

    Fmi::Astronomy::solar_position_t sp =
        Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
    if (sp.dark())
      return 100 + symbol;
    return symbol;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
          return u8"klart";
        case 2:
          return u8"halvklart";
        case 3:
          return u8"mulet";
        case 21:
          return u8"l\u00e4tta regnskurar";
        case 22:
          return u8"regnskurar";
        case 23:
          return u8"kraftiga regnskurar";
        case 31:
          return u8"l\u00e4tt regn";
        case 32:
          return u8"regn";
        case 33:
          return u8"rikligt regn";
        case 41:
          return u8"l\u00e4tta sn\u00f6byar";
        case 42:
          return u8"sn\u00f6byar";
        case 43:
          return u8"t\u00e4ta sn\u00f6byar";
        case 51:
          return u8"l\u00e4tt sn\u00f6fall";
        case 52:
          return u8"sn\u00f6fall";
        case 53:
          return u8"ymnigt sn\u00f6fall";
        case 61:
          return u8"\00e5skskurar";
        case 62:
          return u8"kraftiga \00e5skskurar";
        case 63:
          return u8"\00e5ska";
        case 64:
          return u8"h\u00e4ftigt \00e5skv\u00e4der";
        case 71:
          return u8"l\u00e4tta skurar av sn\u00f6blandat regn";
        case 72:
          return u8"skurar av sn\u00f6blandat regn";
        case 73:
          return u8"kraftiga skurar av sn\u00f6blandad regn";
        case 81:
          return u8"l\u00e4tt sn\u00f6blandat regn";
        case 82:
          return u8"sn\u00f6blandat regn";
        case 83:
          return u8"kraftigt sn\u00f6blandat regn";
        case 91:
          return u8"dis";
        case 92:
          return u8"dimma";
      }
    }
    else if (lang == "et")
    {
      switch (boost::numeric_cast<int>(w))
      {
        case 1:
          return u8"selge";
        case 2:
          return u8"poolpilves";
        case 3:
          return u8"pilves";
        case 21:
          return u8"kerged vihmahood";
        case 22:
          return u8"hoogvihm";
        case 23:
          return u8"tugevad vihmahood";
        case 31:
          return u8"n\u00f5rk vihmasadu";
        case 32:
          return u8"vihmasadu";
        case 33:
          return u8"vihmasadu";
        case 41:
          return u8"n\u00f5rgad lumehood";
        case 42:
          return u8"hooglumi";
        case 43:
          return u8"tihedad lumesajuhood";
        case 51:
          return u8"n\u00f5rk lumesadu";
        case 52:
          return u8"lumesadu";
        case 53:
          return u8"tihe lumesadu";
        case 61:
          return u8"\u00e4ikesehood";
        case 62:
          return u8"tugevad \u00e4ikesehood";
        case 63:
          return u8"\u00e4ike";
        case 64:
          return u8"tugev \u00e4ike";
        case 71:
          return u8"\u00f1\u00f6rgad l\u00f6rtsihood";
        case 72:
          return u8"l\u00f6rtsihood";
        case 73:
          return u8"tugev l\u00f6rtsihood";
        case 81:
          return u8"n\u00f5rk l\u00f6rtsisadu";
        case 82:
          return u8"l\u00f6rtsisadu";
        case 83:
          return u8"tugev l\u00f6rtsisadu";
        case 91:
          return u8"udu";
        case 92:
          return u8"uduvinet";
      }
    }
    else
    {
      switch (boost::numeric_cast<int>(w))
      {
        case 1:
          return u8"selke\u00e4\u00e4";
        case 2:
          return u8"puolipilvist\u00e4";
        case 3:
          return u8"pilvist\u00e4";
        case 21:
          return u8"heikkoja sadekuuroja";
        case 22:
          return u8"sadekuuroja";
        case 23:
          return u8"voimakkaita sadekuuroja";
        case 31:
          return u8"heikkoa vesisadetta";
        case 32:
          return u8"vesisadetta";
        case 33:
          return u8"voimakasta vesisadetta";
        case 41:
          return u8"heikkoja lumikuuroja";
        case 42:
          return u8"lumikuuroja";
        case 43:
          return u8"voimakkaita lumikuuroja";
        case 51:
          return u8"heikkoa lumisadetta";
        case 52:
          return u8"lumisadetta";
        case 53:
          return u8"voimakasta lumisadetta";
        case 61:
          return u8"ukkoskuuroja";
        case 62:
          return u8"voimakkaita ukkoskuuroja";
        case 63:
          return u8"ukkosta";
        case 64:
          return u8"voimakasta ukkosta";
        case 71:
          return u8"heikkoja r\u00e4nt\u00e4kuuroja";
        case 72:
          return u8"r\u00e4nt\u00e4kuuroja";
        case 73:
          return u8"voimakkaita r\u00e4nt\u00e4kuuroja";
        case 81:
          return u8"heikkoa r\u00e4nt\u00e4sadetta";
        case 82:
          return u8"r\u00e4nt\u00e4sadetta";
        case 83:
          return u8"voimakasta r\u00e4nt\u00e4sadetta";
        case 91:
          return u8"utua";
        case 92:
          return u8"sumua";
      }
    }

    return Spine::TimeSeries::None();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, u8"Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate the smart weather symbol if possible
 */
// ----------------------------------------------------------------------

boost::optional<int> calc_smart_symbol(QImpl &q,
                                       const NFmiPoint &latlon,
                                       const boost::local_time::local_date_time &ldt)
{
  try
  {
    // Cloudiness is almost always needed

    if (!q.param(kFmiTotalCloudCover))
      return {};

    const auto n = q.interpolate(latlon, ldt, maxgap);

    if (n == kFloatMissing)
      return {};

    // The first parameter we need always is POT. We allow it to be missing though.

    if (q.param(kFmiProbabilityThunderstorm))
    {
      const auto thunder = q.interpolate(latlon, ldt, maxgap);

      if (thunder >= thunder_limit1 && thunder != kFloatMissing)
      {
        int nclass = (n < cloud_limit6 ? 0 : (n < cloud_limit8 ? 1 : 2));
        return 71 + 3 * nclass;  // 71,74,77
      }
    }

    // No thunder (or not available). Then we always need precipitation rate

    if (!q.param(kFmiPrecipitation1h))
      return {};

    const auto rain = q.interpolate(latlon, ldt, maxgap);

    if (rain == kFloatMissing)
      return {};

    if (rain < rain_limit1)
    {
      // No precipitation. Now we need only fog/cloudiness

      if (q.param(kFmiFogIntensity))
      {
        const auto fog = q.interpolate(latlon, ldt, maxgap);
        if (fog > 0 && fog != kFloatMissing)
          return 9;  // fog
      }

      // no rain, no fog (or not available), only cloudiness
      if (n < cloud_limit2)
        return 1;  // clear
      if (n < cloud_limit3)
        return 2;  // mostly clear
      if (n < cloud_limit6)
        return 4;  // partly cloudy
      if (n < cloud_limit8)
        return 6;  // mostly cloudy
      return 7;    // overcast
    }

    // Since we have precipitation, we always need precipitation form
    int rform = static_cast<int>(kFloatMissing);
    if (q.param(kFmiPotentialPrecipitationForm))
      rform = static_cast<int>(q.interpolate(latlon, ldt, maxgap));
    else if (q.param(kFmiPrecipitationForm))
      rform = static_cast<int>(q.interpolate(latlon, ldt, maxgap));

    if (rform == static_cast<int>(kFloatMissing))
      return {};

    if (rform == 0)  // drizzle
      return 11;

    if (rform == 4)  // freezing drizzle
      return 14;

    if (rform == 5)  // freezing rain
      return 17;

    if (rform == 7 || rform == 8)  // snow or ice particles
      return 57;                   // convert to plain snowfall + cloudy

    // only water, sleet and snow left. Cloudiness limits
    // are the same for them, precipitation limits are not.

    int nclass = (n < cloud_limit6 ? 0 : (n < cloud_limit8 ? 1 : 2));

    if (rform == 6)  // hail
      return 61 + 3 * nclass;

    if (rform == 1)  // water
    {
      // Now we need precipitation type too
      int rtype = 1;  // large scale by default
      if (q.param(kFmiPotentialPrecipitationType))
        rtype = static_cast<int>(q.interpolate(latlon, ldt, maxgap));
      else if (q.param(kFmiPrecipitationType))
        rtype = static_cast<int>(q.interpolate(latlon, ldt, maxgap));

      if (rtype == 2)            // convective
        return 21 + 3 * nclass;  // 21, 24, 27 for showers

      // rtype=1:large scale precipitation (or rtype is missing)
      int rclass = (rain < rain_limit3 ? 0 : (rain < rain_limit6 ? 1 : 2));
      return 31 + 3 * nclass + rclass;  // 31-39 for precipitation
    }

    // rform=2:sleet and rform=3:snow map to 41-49 and 51-59 respectively

    int rclass = (rain < rain_limit3 ? 0 : (rain < rain_limit4 ? 1 : 2));
    return (10 * rform + 21 + 3 * nclass + rclass);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate the weather number used as basis for SmartSymbol
 */
// ----------------------------------------------------------------------

boost::optional<int> calc_weather_number(QImpl &q,
                                         const NFmiPoint &latlon,
                                         const boost::local_time::local_date_time &ldt)
{
  try
  {
    // Cloudiness is optional
    float n = kFloatMissing;
    if (q.param(kFmiTotalCloudCover))
      n = q.interpolate(latlon, ldt, maxgap);

    int n_class = 9;  // missing
    if (n == kFloatMissing)
      n_class = 9;
    else if (n < cloud_limit1)
      n_class = 0;
    else if (n < cloud_limit2)
      n_class = 1;
    else if (n < cloud_limit3)
      n_class = 2;
    else if (n < cloud_limit4)
      n_class = 3;
    else if (n < cloud_limit5)
      n_class = 4;
    else if (n < cloud_limit6)
      n_class = 5;
    else if (n < cloud_limit7)
      n_class = 6;
    else if (n < cloud_limit8)
      n_class = 7;
    else
      n_class = 8;

    // Precipitation is optional
    float rain = kFloatMissing;
    if (q.param(kFmiPrecipitation1h))
      rain = q.interpolate(latlon, ldt, maxgap);

    int rain_class = 9;  // missing
    if (rain == kFloatMissing)
      rain_class = 9;
    else if (rain < rain_limit1)
      rain_class = 0;
    else if (rain < rain_limit2)
      rain_class = 1;
    else if (rain < rain_limit3)
      rain_class = 2;
    else if (rain < rain_limit4)
      rain_class = 3;
    else if (rain < rain_limit5)
      rain_class = 4;
    else if (rain < rain_limit6)
      rain_class = 5;
    else if (rain < rain_limit7)
      rain_class = 6;
    else
      rain_class = 7;

    // Precipitation form is optional
    float rform = kFloatMissing;
    if (q.param(kFmiPotentialPrecipitationForm))
      rform = q.interpolate(latlon, ldt, maxgap);
    else if (q.param(kFmiPrecipitationForm))
      rform = q.interpolate(latlon, ldt, maxgap);

    int rform_class = (rform == kFloatMissing ? 9 : static_cast<int>(rform));

    // Precipitation type is optional
    float rtype = kFloatMissing;
    if (q.param(kFmiPotentialPrecipitationType))
      rtype = q.interpolate(latlon, ldt, maxgap);
    else if (q.param(kFmiPrecipitationType))
      rtype = q.interpolate(latlon, ldt, maxgap);

    int rtype_class = (rtype == kFloatMissing ? 9 : static_cast<int>(rtype));

    // Thunder is optional
    float thunder = kFloatMissing;
    if (q.param(kFmiProbabilityThunderstorm))
      thunder = q.interpolate(latlon, ldt, maxgap);

    int thunder_class = 9;
    if (thunder == kFloatMissing)
      thunder_class = 9;
    else if (thunder < thunder_limit1)
      thunder_class = 0;
    else if (thunder < thunder_limit2)
      thunder_class = 1;
    else
      thunder_class = 2;

    // Fog is optional
    float fog = kFloatMissing;
    if (q.param(kFmiFogIntensity))
      fog = q.interpolate(latlon, ldt, maxgap);

    int fog_class = (fog == kFloatMissing ? 9 : static_cast<int>(fog));

    // Build the number
    const int version = 1;
    const int cloud_class = 0;  // not available yet

    // clang-format off
    return (10000000 * version +
            1000000 * thunder_class +
            100000 * rform_class +
            10000 * rtype_class +
            1000 * rain_class +
            100 * fog_class +
            10 * n_class +
            cloud_class);
    // clang-format on
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}  // namespace Querydata

// ----------------------------------------------------------------------
/*!
 * \brief SmartSymbol
 */
// ----------------------------------------------------------------------

ts::Value SmartSymbolNumber(QImpl &q,
                            const Spine::Location &loc,
                            const boost::local_time::local_date_time &ldt)
{
  try
  {
    NFmiPoint latlon(loc.longitude, loc.latitude);

    auto symbol = calc_smart_symbol(q, latlon, ldt);

    if (!symbol || *symbol == kFloatMissing)
      return Spine::TimeSeries::None();

    // Add day/night information
    Fmi::Astronomy::solar_position_t sp =
        Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);

    if (sp.dark())
      return 100 + *symbol;
    return *symbol;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WeatherNumber
 */
// ----------------------------------------------------------------------

ts::Value WeatherNumber(QImpl &q,
                        const Spine::Location &loc,
                        const boost::local_time::local_date_time &ldt)
{
  try
  {
    NFmiPoint latlon(loc.longitude, loc.latitude);

    auto number = calc_weather_number(q, latlon, ldt);

    if (!number)
      return Spine::TimeSeries::None();

    return *number;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Symbol text
 */
// ----------------------------------------------------------------------

ts::Value SmartSymbolText(QImpl &q,
                          const Spine::Location &loc,
                          const boost::local_time::local_date_time &ldt,
                          const std::string &lang)
{
  try
  {
    NFmiPoint latlon(loc.longitude, loc.latitude);

    auto symbol = calc_smart_symbol(q, latlon, ldt);

    if (!symbol)
      return Spine::TimeSeries::None();

    if (lang == "en")
    {
      switch (*symbol)
      {
        case 1:
          return "clear";
        case 2:
          return "mostly clear";
        case 4:
          return "partly cloudy";
        case 6:
          return "mostly cloudy";
        case 7:
          return "overcast";
        case 9:
          return "fog";
        case 71:
          return "isolated thundershowers";
        case 74:
          return "scattered thundershowers";
        case 77:
          return "thundershowers";
        case 21:
          return "isolated showers";
        case 24:
          return "scattered showers";
        case 27:
          return "showers";
        case 11:
          return "drizzle";
        case 14:
          return "freezing drizzle";
        case 17:
          return "freezing rain";
        case 31:
          return "periods of light rain";
        case 34:
          return "periods of light rain";
        case 37:
          return "light rain";
        case 32:
          return "periods of moderate rain";
        case 35:
          return "periods of moderate rain";
        case 38:
          return "moderate rain";
        case 33:
          return "periods of heavy rain";
        case 36:
          return "periods of heavy rain";
        case 39:
          return "heavy rain";
        case 41:
          return "isolated light sleet showers";
        case 44:
          return "scattered light sleet showers";
        case 47:
          return "light sleet";
        case 42:
          return "isolated moderate sleet showers";
        case 45:
          return "scattered moderate sleet showers";
        case 48:
          return "moderate sleet";
        case 43:
          return "isolated heavy sleet showers";
        case 46:
          return "scattered heavy sleet showers";
        case 49:
          return "heavy sleet";
        case 51:
          return "isolated light snow showers";
        case 54:
          return "scattered light snow showers";
        case 57:
          return "light snowfall";
        case 52:
          return "isolated moderate snow showers";
        case 55:
          return "scattered moderate snow showers";
        case 58:
          return "moderate snowfall";
        case 53:
          return "isolated heavy snow showers";
        case 56:
          return "scattered heavy snow showers";
        case 59:
          return "heavy snowfall";
        case 61:
          return "isolated hail showers";
        case 64:
          return "scattered hail showers";
        case 67:
          return "hail showers";
      }
    }
    else if (lang == "sv")
    {
      // From http://sv.ilmatieteenlaitos.fi/vadersymbolerna
      switch (*symbol)
      {
        case 1:
          return u8"klart";
        case 2:
          return u8"mest klart";
        case 4:
          return u8"halvklart";
        case 6:
          return u8"molnight";
        case 7:
          return u8"mulet";
        case 9:
          return u8"dimma";
        case 71:
          return u8"enstaka \00e5skskurar";
        case 74:
          return u8"lokalt \00e5skskurar";
        case 77:
          return u8"\00e5skskurar";
        case 21:
          return u8"enstaka regnskurar";
        case 24:
          return u8"lokalt regnskurar";
        case 27:
          return u8"regnskurar";
        case 11:
          return u8"duggregn";
        case 14:
          return u8"underkylt duggregn";
        case 17:
          return u8"underkylt regn";
        case 31:
          return u8"tidvis l\u00e4tt regn";
        case 34:
          return u8"tidvis l\u00e4tt regn";
        case 37:
          return u8"l\u00e4tt regn";
        case 32:
          return u8"tidvis m\00e5ttligt regn";
        case 35:
          return u8"tidvis m\00e5ttligt regn";
        case 38:
          return u8"m\00e5ttligt regn";
        case 33:
          return u8"tidvis kraftigt regn";
        case 36:
          return u8"tidvis kraftigt regn";
        case 39:
          return u8"kraftigt regn";
        case 41:
          return u8"tidvis l\u00e4tta byar ov sn\u00f6blandat regn";
        case 44:
          return u8"tidvis l\u00e4tta byar avd sn\u00e4blandat regn";
        case 47:
          return u8"l\u00e4tt sn\u00f6blandat regn";
        case 42:
          return u8"tidvis m\00e5ttliga byar av sn\u00f6blandat regn";
        case 45:
          return u8"tidvis m\00e5ttliga byar av sn\u00f6blandat regn";
        case 48:
          return u8"m\00e5ttligt sn\u00f6blandat regn";
        case 43:
          return u8"tidvis kraftiga byar av sn\u00f6blandat regn";
        case 46:
          return u8"tidvis kraftiga byar av sn\u00f6blandat regn";
        case 49:
          return u8"kraftigt sn\u00f6blandat regn";
        case 51:
          return u8"tidvis l\u00e4tta sn\u00f6byar";
        case 54:
          return u8"tidvis l\u00e4tta sn\u00f6byar";
        case 57:
          return u8"tidvis l\u00e4tt sn\u00f6fall";
        case 52:
          return u8"tidvis m\00e5ttliga sn\u00f6byar";
        case 55:
          return u8"tidvis m\00e5ttliga sn\u00f6byar";
        case 58:
          return u8"m\00e5ttligt sn\u00f6fall";
        case 53:
          return u8"tidvis ymniga sn\u00f6byar";
        case 56:
          return u8"tidvis ymniga sn\u00f6byar";
        case 59:
          return u8"ymnigt sn\u00f6fall";
        case 61:
          return u8"enstaka hagelskurar";
        case 64:
          return u8"lokalt hagelskurar";
        case 67:
          return u8"hagelskurar";
      }
    }
    else
    {
      switch (*symbol)
      {
        case 1:
          return u8"selke\u00e4\u00e4";
        case 2:
          return u8"enimm\u00e4kseen selke\u00e4\u00e4";
        case 4:
          return u8"puolipilvist\u00e4";
        case 6:
          return u8"enimm\u00e4kseen pilvist\u00e4";
        case 7:
          return u8"pilvist\u00e4";
        case 9:
          return u8"sumua";
        case 71:
          return u8"yksitt\u00e4isi\u00e4 ukkoskuuroja";
        case 74:
          return u8"paikoin ukkoskuuroja";
        case 77:
          return u8"ukkoskuuroja";
        case 21:
          return u8"yksitt\u00e4isi\u00e4 sadekuuroja";
        case 24:
          return u8"paikoin sadekuuroja";
        case 27:
          return u8"sadekuuroja";
        case 11:
          return u8"tihkusadetta";
        case 14:
          return u8"j\u00e4\u00e4t\u00e4v\u00e4\u00e4 tihkua";
        case 17:
          return u8"j\u00e4\u00e4t\u00e4v\u00e4\u00e4 sadetta";
        case 31:
          return u8"ajoittain heikkoa vesisadetta";
        case 34:
          return u8"ajoittain heikkoa vesisadetta";
        case 37:
          return u8"heikkoa vesisadetta";
        case 32:
          return u8"ajoittain kohtalaista vesisadetta";
        case 35:
          return u8"ajoittain kohtalaista vesisadetta";
        case 38:
          return u8"kohtalaista vesisadetta";
        case 33:
          return u8"ajoittain voimakasta vesisadetta";
        case 36:
          return u8"ajoittain voimakasta vesisadetta";
        case 39:
          return u8"voimakasta vesisadetta";
        case 41:
          return u8"ajoittain heikkoja r\u00e4nt\u00e4kuuroja";
        case 44:
          return u8"ajoittain heikkoja r\u00e4nt\u00e4kuuroja";
        case 47:
          return u8"heikkoa r\u00e4nt\u00e4sadetta";
        case 42:
          return u8"ajoittain kohtalaisia r\u00e4nt\u00e4kuuroja";
        case 45:
          return u8"ajoittain kohtalaisia r\u00e4nt\u00e4kuuroja";
        case 48:
          return u8"kohtalaista r\u00e4nt\u00e4sadetta";
        case 43:
          return u8"ajoittain voimakkaita r\u00e4nt\u00e4kuuroja";
        case 46:
          return u8"ajoittain voimakkaita r\u00e4nt\u00e4kuuroja";
        case 49:
          return u8"voimakasta r\u00e4nt\u00e4sadetta";
        case 51:
          return u8"ajoittain heikkoja lumikuuroja";
        case 54:
          return u8"ajoittain heikkoja lumikuuroja";
        case 57:
          return u8"heikkoa lumisadetta";
        case 52:
          return u8"ajoittain kohtalaisia lumikuuroja";
        case 55:
          return u8"ajoittain kohtalaisia lumikuuroja";
        case 58:
          return u8"kohtalaista lumisadetta";
        case 53:
          return u8"ajoittain sakeita lumikuuroja";
        case 56:
          return u8"ajoittain sakeita lumikuuroja";
        case 59:
          return u8"runsasta lumisadetta";
        case 61:
          return u8"yksitt\u00e4isi\u00e4 raekuuroja";
        case 64:
          return u8"paikoin raekuuroja";
        case 67:
          return u8"raekuuroja";
      }
    }
    throw Spine::Exception(BCP, "Unknown symbol value : " + Fmi::to_string(*symbol));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Grid north deviation
 */
// ----------------------------------------------------------------------

ts::Value GridNorth(const QImpl &q, const Spine::Location &loc)
{
  try
  {
    if (!q.isArea())
      return Spine::TimeSeries::None();

    NFmiPoint latlon(loc.longitude, loc.latitude);
    return q.area().TrueNorthAzimuth(latlon).Value();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract data value
 */
// ----------------------------------------------------------------------

ts::Value QImpl::dataValue(const ParameterOptions &opt,
                           const NFmiPoint &latlon,
                           const boost::local_time::local_date_time &ldt)
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
    return Spine::TimeSeries::None();

  return interpolatedValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract data independent parameter value
 */
// ----------------------------------------------------------------------

ts::Value QImpl::dataIndependentValue(const ParameterOptions &opt,
                                      const boost::local_time::local_date_time &ldt,
                                      double levelResult)
{
  // Some shorthand variables
  const std::string &pname = opt.par.name();
  const Spine::Location &loc = opt.loc;

  if (pname == "place")
    return opt.place;

  if (pname == "name")
    return loc.name;

  if (pname == "iso2")
    return loc.iso2;

  if (pname == "geoid")
  {
    if (loc.geoid == 0)  // not sure why this is still here
      return Spine::TimeSeries::None();
    return Fmi::to_string(loc.geoid);
  }

  // We allow coordinates to be both DataIndependent and DataDerived depending
  // on the needs of plugins
  if (pname == "latitude" || pname == "lat")
    return loc.latitude;

  if (pname == "longitude" || pname == "lon")
    return loc.longitude;

  if (pname == "latlon" || pname == "lonlat")
    return ts::LonLat(loc.longitude, loc.latitude);

  if (pname == "region")
  {
    // This reintroduces an older bug/feature where
    // the name of the location is given as a region
    // if it doesn't belong to any administrative region.
    // (i.e. Helsinki doesn't have region, Kumpula has.)
    //
    // Also checking whether the loc.name has valid data,
    // if it's empty as well - which shoudn't occur - we return nan

    if (!loc.area.empty())
      return loc.area;  // Administrative region known.

    if (loc.name.empty())
      // No area (administrative region) nor name known.
      return Spine::TimeSeries::None();

    // Place name known, administrative region unknown.
    return loc.name;
  }

  if (pname == "country")
    return opt.country;

  if (pname == "feature")
    return loc.feature;

  if (pname == "tz")
  {
    if (ldt.zone())
      return ldt.zone()->std_zone_name();
    return Spine::TimeSeries::None();
  }

  if (pname == "localtz")
    return loc.timezone;

  if (pname == "level")
    return levelResult;

  if (pname == "nearlatitude")
    return opt.lastpoint.Y();

  if (pname == "nearlongitude")
    return opt.lastpoint.X();

  if (pname == "nearlatlon" || pname == "nearlonlat")
    return ts::LonLat(opt.lastpoint.X(), opt.lastpoint.Y());

  if (pname == "population")
    return Fmi::to_string(loc.population);

  if (pname == "elevation")
    return Fmi::to_string(loc.elevation);

  if (pname == "dem")
    return Fmi::to_string(loc.dem);

  if (pname == "covertype")
    return Fmi::to_string(static_cast<int>(loc.covertype));

  if (pname == "model")
    return opt.producer;

  if (pname == "time")
    return opt.timeformatter.format(ldt);

  if (pname == "isotime")
    return Fmi::to_iso_string(ldt.local_time());

  if (pname == "xmltime")
    return Fmi::to_iso_extended_string(ldt.local_time());

  if (pname == "localtime")
  {
    auto localtz = Fmi::TimeZoneFactory::instance().time_zone_from_string(loc.timezone);
    boost::posix_time::ptime utc = ldt.utc_time();
    boost::local_time::local_date_time localt(utc, localtz);
    return opt.timeformatter.format(localt);
  }

  if (pname == "utctime")
    return opt.timeformatter.format(ldt.utc_time());

  if (pname == "epochtime")
  {
    boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
    boost::posix_time::time_duration diff = ldt.utc_time() - time_t_epoch;
    return Fmi::to_string(diff.total_seconds());
  }

  if (pname == "origintime")
  {
    if (!time(ldt.utc_time()))
    {
      // Search first valid time after the desired time, and choose that origintime
      bool ok = false;
      for (resetTime(); !ok && nextTime();)
        ok = (validTime() > ldt.utc_time());
      if (!ok)
        return Spine::TimeSeries::None();
    }

    boost::posix_time::ptime utc = originTime();
    boost::local_time::local_date_time localt(utc, ldt.zone());
    return opt.timeformatter.format(localt);
  }

  if (pname == "modtime" || pname == "mtime")
  {
    boost::posix_time::ptime utc = modificationTime();
    boost::local_time::local_date_time localt(utc, ldt.zone());
    return opt.timeformatter.format(localt);
  }

  if (pname == "dark")
  {
    auto pos = Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(pos.dark()));
  }

  if (pname == "moonphase")
    return Fmi::Astronomy::moonphase(ldt.utc_time());

  if (pname == "moonrise")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_iso_string(ltime.moonrise.local_time());
  }
  if (pname == "moonrise2")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

    if (ltime.moonrise2_today())
      return Fmi::to_iso_string(ltime.moonrise2.local_time());

    return std::string("");
  }
  if (pname == "moonset")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_iso_string(ltime.moonset.local_time());
  }
  if (pname == "moonset2")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    if (ltime.moonset2_today())
      return Fmi::to_iso_string(ltime.moonset2.local_time());
    return std::string("");
  }
  if (pname == "moonrisetoday")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(ltime.moonrise_today()));
  }
  if (pname == "moonrise2today")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(ltime.moonrise2_today()));
  }
  if (pname == "moonsettoday")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(ltime.moonset_today()));
  }
  if (pname == "moonset2today")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(ltime.moonset2_today()));
  }
  if (pname == "moonup24h")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(ltime.above_horizont_24h()));
  }
  if (pname == "moondown24h")
  {
    auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(!ltime.moonrise_today() && !ltime.moonset_today() &&
                                           !ltime.above_horizont_24h()));
  }

  if (pname == "sunrise")
  {
    auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_iso_string(stime.sunrise.local_time());
  }
  if (pname == "sunset")
  {
    auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_iso_string(stime.sunset.local_time());
  }
  if (pname == "noon")
  {
    auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_iso_string(stime.noon.local_time());
  }
  if (pname == "sunrisetoday")
  {
    auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(stime.sunrise_today()));
  }
  if (pname == "sunsettoday")
  {
    auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
    return Fmi::to_string(static_cast<int>(stime.sunset_today()));
  }
  if (pname == "daylength")
  {
    auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
    auto seconds = stime.daylength().total_seconds();
    auto minutes = lround(seconds / 60.0);
    return Fmi::to_string(minutes);
  }
  if (pname == "timestring")
    return format_date(ldt, opt.outlocale, opt.timestring);

  if (pname == "wday")
    return format_date(ldt, opt.outlocale, "%a");

  if (pname == "weekday")
    return format_date(ldt, opt.outlocale, "%A");

  if (pname == "mon")
    return format_date(ldt, opt.outlocale, "%b");

  if (pname == "month")
    return format_date(ldt, opt.outlocale, "%B");

  if (pname == "hour")
    return Fmi::to_string(ldt.local_time().time_of_day().hours());

  if (pname.substr(0, 5) == "date(" && pname[pname.size() - 1] == ')')
    return format_date(ldt, opt.outlocale, pname.substr(5, pname.size() - 6));

  if (pname == "sunelevation")
  {
    auto pos = Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
    return pos.elevation;
  }

  if (pname == "sundeclination")
  {
    auto pos = Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
    return pos.declination;
  }

  if (pname == "sunazimuth")
  {
    auto pos = Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
    return pos.azimuth;
  }

  if (pname == "gridnorth")
    return GridNorth(*this, loc);

  // The following parameters are added for for obsengine compability reasons
  // so that we can have e.g. fmisid identifier for observations in query which
  // has both observations and forecasts
  if (pname == "fmisid" || pname == "wmo" || pname == "lpnn" || pname == "rwsid" ||
      pname == "stationary" || pname == "distance" || pname == "direction" ||
      pname == "sensor_no" || pname == "stationname")
    return Spine::TimeSeries::None();

  throw Spine::Exception(BCP, "Unknown DataIndependent special function '" + pname + "'!");
}

// ======================================================================

ts::Value QImpl::value(const ParameterOptions &opt, const boost::local_time::local_date_time &ldt)
{
  try
  {
    // Default return value
    ts::Value retval = Spine::TimeSeries::None();

    // Some shorthand variables
    std::string pname = boost::algorithm::to_lower_copy(opt.par.name(), std::locale::classic());
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
        // normal handling continues below
      }
      // fall through
      case Spine::Parameter::Type::Data:
      {
        opt.lastpoint = latlon;
        if (param(opt.par.number()))
          retval = dataValue(opt, latlon, ldt);
        break;
      }
      case Spine::Parameter::Type::DataDerived:
      {
        if (pname == "latitude" || pname == "lat")
          retval = loc.latitude;

        else if (pname == "longitude" || pname == "lon")
          retval = loc.longitude;

        else if (pname == "latlon" || pname == "lonlat")
          retval = ts::LonLat(loc.longitude, loc.latitude);

        else if (pname == "windcompass8")
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

        else if (pname == "smartsymbol")
          retval = SmartSymbolNumber(*this, loc, ldt);

        else if (pname == "smartsymboltext")
          retval = SmartSymbolText(*this, loc, ldt, opt.language);

        else if (pname == "weathernumber")
          retval = WeatherNumber(*this, loc, ldt);

        else if (pname == "snow1hlower")
          retval = Snow1hLower(*this, loc, ldt);

        else if (pname == "snow1hupper")
          retval = Snow1hUpper(*this, loc, ldt);

        else if (pname == "snow1h")
          retval = Snow1h(*this, loc, ldt);

        else if (pname == "windums")
        {
          if (isRelativeUV())
            retval = WindUMS(*this, loc, ldt);
          else if (param(kFmiWindUMS))
            retval = dataValue(opt, latlon, ldt);
        }

        else if (pname == "windvms")
        {
          if (isRelativeUV())
            retval = WindVMS(*this, loc, ldt);
          else if (param(kFmiWindVMS))
            retval = dataValue(opt, latlon, ldt);
        }

        else
          throw Spine::Exception(BCP, "Unknown DataDerived parameter '" + pname + "'!");

        break;
      }
      case Spine::Parameter::Type::DataIndependent:
      {
        retval = dataIndependentValue(opt, ldt, levelValue());
        break;
      }
    }

    if (boost::get<double>(&retval) != nullptr)
    {
      if (*(boost::get<double>(&retval)) == kFloatMissing)
        retval = Spine::TimeSeries::None();
    }

    return retval;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ts::Value QImpl::valueAtPressure(const ParameterOptions &opt,
                                 const boost::local_time::local_date_time &ldt,
                                 float pressure)
{
  try
  {
    // Default return value
    ts::Value retval = Spine::TimeSeries::None();

    // Some shorthand variables
    const Spine::Location &loc = opt.loc;
    std::string pname = boost::algorithm::to_lower_copy(opt.par.name(), std::locale::classic());

    // Update last accessed point.

    NFmiPoint latlon(loc.longitude, loc.latitude);

    switch (opt.par.type())
    {
      case Spine::Parameter::Type::Landscaped:
      case Spine::Parameter::Type::Data:
      {
        opt.lastpoint = latlon;

        if (param(opt.par.number()) && (itsModels[0]->levelName() != "surface") && !isClimatology())
        {
          NFmiMetTime t = ldt;

          float interpolatedValue = interpolateAtPressure(latlon, t, pressure, maxgap);

          // If we got no value and the proper flag is on,
          // find the nearest point with valid values and use
          // the values from that point

          if (interpolatedValue == kFloatMissing && opt.findnearestvalidpoint)
          {
            interpolatedValue = interpolateAtPressure(opt.nearestpoint, t, pressure, maxgap);
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
        if (pname == "latitude" || pname == "lat")
          retval = loc.latitude;

        else if (pname == "longitude" || pname == "lon")
          retval = loc.longitude;

        else if (pname == "latlon" || pname == "lonlat")
          retval = ts::LonLat(loc.longitude, loc.latitude);

        break;
      }
      case Spine::Parameter::Type::DataIndependent:
      {
        retval = dataIndependentValue(opt, ldt, pressure);
        break;
      }
    }

    if (boost::get<double>(&retval) != nullptr)
    {
      if (*(boost::get<double>(&retval)) == kFloatMissing)
        retval = Spine::TimeSeries::None();
    }

    return retval;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ts::Value QImpl::valueAtHeight(const ParameterOptions &opt,
                               const boost::local_time::local_date_time &ldt,
                               float height)
{
  try
  {
    // Default return value
    ts::Value retval = Spine::TimeSeries::None();

    // Some shorthand variables
    const Spine::Location &loc = opt.loc;
    std::string pname = boost::algorithm::to_lower_copy(opt.par.name(), std::locale::classic());

    // Update last accessed point.

    NFmiPoint latlon(loc.longitude, loc.latitude);

    switch (opt.par.type())
    {
      case Spine::Parameter::Type::Landscaped:
      case Spine::Parameter::Type::Data:
      {
        opt.lastpoint = latlon;

        if (param(opt.par.number()) && (itsModels[0]->levelName() != "surface") && !isClimatology())
        {
          NFmiMetTime t = ldt;

          float interpolatedValue = interpolateAtHeight(latlon, t, height, maxgap);

          // If we got no value and the proper flag is on,
          // find the nearest point with valid values and use
          // the values from that point

          if (interpolatedValue == kFloatMissing && opt.findnearestvalidpoint)
          {
            interpolatedValue = interpolateAtHeight(opt.nearestpoint, t, height, maxgap);
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
        if (pname == "latitude" || pname == "lat")
          retval = loc.latitude;

        else if (pname == "longitude" || pname == "lon")
          retval = loc.longitude;

        else if (pname == "latlon" || pname == "lonlat")
          retval = ts::LonLat(loc.longitude, loc.latitude);

        break;
      }
      case Spine::Parameter::Type::DataIndependent:
      {
        retval = dataIndependentValue(opt, ldt, height);
        break;
      }
    }

    if (boost::get<double>(&retval) != nullptr)
    {
      if (*(boost::get<double>(&retval)) == kFloatMissing)
        retval = Spine::TimeSeries::None();
    }

    return retval;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// one location, many timesteps
ts::TimeSeriesPtr QImpl::values(const ParameterOptions &param,
                                const Spine::TimeSeriesGenerator::LocalTimeList &tlist)
{
  try
  {
    ts::TimeSeriesPtr ret(new ts::TimeSeries);

    for (const boost::local_time::local_date_time &ldt : tlist)
    {
      ret->push_back(ts::TimedValue(ldt, value(param, ldt)));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
ts::TimeSeriesPtr QImpl::valuesAtPressure(const ParameterOptions &param,
                                          const Spine::TimeSeriesGenerator::LocalTimeList &tlist,
                                          float pressure)
{
  try
  {
    ts::TimeSeriesPtr ret(new ts::TimeSeries);

    for (const boost::local_time::local_date_time &ldt : tlist)
    {
      ret->push_back(ts::TimedValue(ldt, valueAtPressure(param, ldt, pressure)));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
ts::TimeSeriesPtr QImpl::valuesAtHeight(const ParameterOptions &param,
                                        const Spine::TimeSeriesGenerator::LocalTimeList &tlist,
                                        float height)
{
  try
  {
    ts::TimeSeriesPtr ret(new ts::TimeSeries);

    for (const boost::local_time::local_date_time &ldt : tlist)
    {
      ret->push_back(ts::TimedValue(ldt, valueAtHeight(param, ldt, height)));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// many locations (indexmask), many timesteps
ts::TimeSeriesGroupPtr QImpl::values(const ParameterOptions &param,
                                     const NFmiIndexMask &indexmask,
                                     const Spine::TimeSeriesGenerator::LocalTimeList &tlist)
{
  try
  {
    ts::TimeSeriesGroupPtr ret(new ts::TimeSeriesGroup);

    for (const auto &mask : indexmask)
    {
      // Indexed latlon
      NFmiPoint latlon(latLon(mask));

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
ts::TimeSeriesGroupPtr QImpl::valuesAtPressure(
    const ParameterOptions &param,
    const NFmiIndexMask &indexmask,
    const Spine::TimeSeriesGenerator::LocalTimeList &tlist,
    float pressure)
{
  try
  {
    ts::TimeSeriesGroupPtr ret(new ts::TimeSeriesGroup);

    for (const auto &mask : indexmask)
    {
      // Indexed latlon
      NFmiPoint latlon(latLon(mask));

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

      ts::TimeSeriesPtr timeseries = valuesAtPressure(paramOptions, tlist, pressure);
      ts::LonLat lonlat(latlon.X(), latlon.Y());

      ret->push_back(ts::LonLatTimeSeries(lonlat, *timeseries));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
ts::TimeSeriesGroupPtr QImpl::valuesAtHeight(const ParameterOptions &param,
                                             const NFmiIndexMask &indexmask,
                                             const Spine::TimeSeriesGenerator::LocalTimeList &tlist,
                                             float height)
{
  try
  {
    ts::TimeSeriesGroupPtr ret(new ts::TimeSeriesGroup);

    for (const auto &mask : indexmask)
    {
      // Indexed latlon
      NFmiPoint latlon(latLon(mask));

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

      ts::TimeSeriesPtr timeseries = valuesAtHeight(paramOptions, tlist, height);
      ts::LonLat lonlat(latlon.X(), latlon.Y());

      ret->push_back(ts::LonLatTimeSeries(lonlat, *timeseries));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// many locations (llist), many timesteps

// BUG?? Why is maxdistance in the API?

ts::TimeSeriesGroupPtr QImpl::values(const ParameterOptions &param,
                                     const Spine::LocationList &llist,
                                     const Spine::TimeSeriesGenerator::LocalTimeList &tlist,
                                     const double & /* maxdistance */)
{
  try
  {
    ts::TimeSeriesGroupPtr ret(new ts::TimeSeriesGroup);

    for (const Spine::LocationPtr &loc : llist)
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
ts::TimeSeriesGroupPtr QImpl::valuesAtPressure(
    const ParameterOptions &param,
    const Spine::LocationList &llist,
    const Spine::TimeSeriesGenerator::LocalTimeList &tlist,
    const double & /* maxdistance */,
    float pressure)
{
  try
  {
    ts::TimeSeriesGroupPtr ret(new ts::TimeSeriesGroup);

    for (const Spine::LocationPtr &loc : llist)
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

      ts::TimeSeriesPtr timeseries = valuesAtPressure(paramOptions, tlist, pressure);
      ts::LonLat lonlat(loc->longitude, loc->latitude);

      ret->push_back(ts::LonLatTimeSeries(lonlat, *timeseries));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
ts::TimeSeriesGroupPtr QImpl::valuesAtHeight(const ParameterOptions &param,
                                             const Spine::LocationList &llist,
                                             const Spine::TimeSeriesGenerator::LocalTimeList &tlist,
                                             const double & /* maxdistance */,
                                             float height)
{
  try
  {
    ts::TimeSeriesGroupPtr ret(new ts::TimeSeriesGroup);

    for (const Spine::LocationPtr &loc : llist)
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

      ts::TimeSeriesPtr timeseries = valuesAtHeight(paramOptions, tlist, height);
      ts::LonLat lonlat(loc->longitude, loc->latitude);

      ret->push_back(ts::LonLatTimeSeries(lonlat, *timeseries));
    }

    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    if (theResolution < 0.01)
    {
      if (theResolution > 0)
        throw Spine::Exception(BCP, "Resolutions below 10 meters are not supported!");

      if (theLocationCache.NX() > 0)
        throw Spine::Exception(BCP, "Nonzero resolution must be given with locations!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    tlist.Add(new NFmiMetTime(theTime));  // NOLINT(cppcoreguidelines-owning-memory)
    NFmiTimeDescriptor tdesc(itsInfo->OriginTime(), tlist);

    // Establish new projection and the required grid size of the desired resolution

    boost::shared_ptr<NFmiArea> newarea(
        NFmiArea::CreateFromBBox(theCrs, NFmiPoint(theXmin, theYmin), NFmiPoint(theXmax, theYmax)));

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
    if (data.get() == nullptr)
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

      auto mylocale = std::locale::classic();

      for (dstinfo.ResetLevel(); dstinfo.NextLevel();)
      {
        itsInfo->Level(*dstinfo.Level());
        for (dstinfo.ResetLocation(); dstinfo.NextLocation();)
        {
          auto latlon = dstinfo.LatLon();

          if (theParameter.name() == "dem")
            dstinfo.FloatValue(theDem.elevation(latlon.X(), latlon.Y(), theResolution));
          else if (theParameter.name() == "covertype")
            dstinfo.FloatValue(theLandCover.coverType(latlon.X(), latlon.Y()));
          else
          {
            Spine::Location loc(latlon.X(), latlon.Y());

            ParameterOptions options(theParameter,
                                     Producer(),
                                     loc,
                                     "",
                                     "",
                                     *timeformatter,
                                     "",
                                     "",
                                     mylocale,
                                     "",
                                     false,
                                     NFmiPoint(),
                                     dummy);

            auto result = value(options, localdatetime);
            if (boost::get<double>(&result) != nullptr)
              dstinfo.FloatValue(*boost::get<double>(&result));
          }
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
    CPLFree(tmp);

    auto model = boost::make_shared<Model>(*itsModels[0], data, hash);
    return boost::make_shared<QImpl>(model);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the grid hash value
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
 * \brief Return true if the data looks global but lacks one grid cell column
 */
// ----------------------------------------------------------------------

bool QImpl::needsWraparound() const
{
  if (!isGrid())
    return false;

  NFmiFastQueryInfo &qi = *itsInfo;

  const NFmiArea *area = qi.Area();
  const NFmiGrid *grid = qi.Grid();

  const auto x1 = area->BottomLeftLatLon().X();
  const auto x2 = area->TopRightLatLon().X();

  const auto nx = grid->XNumber();

  if (x1 == kFloatMissing || x2 == kFloatMissing)
    return false;

  /*
   * GFS example:
   * bottom left lonlat= 0,-90
   * top right lonlat= 359.75,90
   * xnumber= 1440
   *
   * ==> (x1-x1)*1441/1440 = 360  ==> we need to generate an extra cell by wrapping around
   */

  auto dx = x2 - x1;  // PROJ.4 may return -0.25 instead of 359.75
  if (dx < 0)
    dx += 360;

  auto test_width = dx * (nx + 1) / nx;

  // In the GFS case the rounding error is about 1e-4
  return (std::abs(test_width - 360) < 1e-3);
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
