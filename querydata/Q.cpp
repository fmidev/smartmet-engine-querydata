#include "Q.h"
#include "Model.h"
#include "WGS84EnvelopeFactory.h"
#include <boost/math/constants/constants.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/timer/timer.hpp>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/DEM.h>
#include <gis/LandCover.h>
#include <gis/OGR.h>
#include <gis/SpatialReference.h>
#include <macgyver/Astronomy.h>
#include <macgyver/CharsetTools.h>
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TimeZoneFactory.h>
#include <newbase/NFmiMetMath.h>
#include <newbase/NFmiMultiQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeList.h>
#include <timeseries/ParameterFactory.h>
#include <cassert>
#include <ogr_spatialref.h>
#include <optional>
#include <stdexcept>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
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

enum class InterpolationMethod
{
  PRESSURE,
  HEIGHT,
  SURFACE
};

const char *level_name(FmiLevelType theLevel)
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
        throw Fmi::Exception(BCP, "Internal error in deducing level names");
#endif
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool is_leap_year(int year)
{
  if (year % 4 != 0)
    return false;
  if (year % 100 == 0)
    return false;
  return true;
}

// Max interpolation gap
const int maxgap = 6 * 60;

// ----------------------------------------------------------------------
/*!
 * \brief Is the location of water type?
 */
// ----------------------------------------------------------------------

#if 0
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Time formatter
 */
// ----------------------------------------------------------------------

std::string format_date(const Fmi::LocalDateTime &ldt,
                        const std::locale &llocale,
                        const std::string &fmt)
{
  try
  {
    const std::string tmp = Fmi::date_time::format_time(llocale, fmt, ldt);
    return Fmi::latin1_to_utf8(tmp);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindUMS with true north orientation
 */
// ----------------------------------------------------------------------

TS::Value WindUMS(QImpl &q,
                  const Spine::Location &loc,
                  const Fmi::LocalDateTime &ldt,
                  std::optional<float> level = std::nullopt,
                  InterpolationMethod method = InterpolationMethod::SURFACE)
{
  try
  {
    Fmi::CoordinateTransformation transformation("WGS84", q.SpatialReference());
    auto opt_angle = Fmi::OGR::gridNorth(transformation, loc.longitude, loc.latitude);

    if (!opt_angle)
      return TS::None();

    auto angle = *opt_angle * boost::math::double_constants::degree;

    NFmiPoint latlon(loc.longitude, loc.latitude);
    // auto angle = q.area().TrueNorthAzimuth(latlon).ToRad();

    if (!q.param(kFmiWindUMS))
      return TS::None();

    auto u = (level ? (method == InterpolationMethod::PRESSURE
                           ? q.interpolateAtPressure(latlon, ldt, maxgap, *level)
                           : q.interpolateAtHeight(latlon, ldt, maxgap, *level))
                    : q.interpolate(latlon, ldt, maxgap));

    if (angle == 0)
      return u;

    if (!q.param(kFmiWindVMS))
      return TS::None();

    auto v = (level ? (method == InterpolationMethod::PRESSURE
                           ? q.interpolateAtPressure(latlon, ldt, maxgap, *level)
                           : q.interpolateAtHeight(latlon, ldt, maxgap, *level))
                    : q.interpolate(latlon, ldt, maxgap));

    if (u == kFloatMissing || v == kFloatMissing)
      return TS::None();

    // Unrotate U by the given angle

    return u * cos(-angle) + v * sin(-angle);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindVMS with true north orientation
 */
// ----------------------------------------------------------------------

TS::Value WindVMS(QImpl &q,
                  const Spine::Location &loc,
                  const Fmi::LocalDateTime &ldt,
                  std::optional<float> level = std::nullopt,
                  InterpolationMethod method = InterpolationMethod::SURFACE)
{
  try
  {
    Fmi::CoordinateTransformation transformation("WGS84", q.SpatialReference());
    auto opt_angle = Fmi::OGR::gridNorth(transformation, loc.longitude, loc.latitude);

    if (!opt_angle)
      return TS::None();

    auto angle = *opt_angle * boost::math::double_constants::degree;

    NFmiPoint latlon(loc.longitude, loc.latitude);
    // auto angle = q.area().TrueNorthAzimuth(latlon).ToRad();

    if (!q.param(kFmiWindVMS))
      return TS::None();

    NFmiMetTime t(ldt);

    auto v = (level ? (method == InterpolationMethod::PRESSURE
                           ? q.interpolateAtPressure(latlon, ldt, maxgap, *level)
                           : q.interpolateAtHeight(latlon, ldt, maxgap, *level))
                    : q.interpolate(latlon, ldt, maxgap));

    if (angle == 0)
      return v;

    if (!q.param(kFmiWindUMS))
      return TS::None();

    auto u = (level ? (method == InterpolationMethod::PRESSURE
                           ? q.interpolateAtPressure(latlon, ldt, maxgap, *level)
                           : q.interpolateAtHeight(latlon, ldt, maxgap, *level))
                    : q.interpolate(latlon, ldt, maxgap));

    if (u == kFloatMissing || v == kFloatMissing)
      return TS::None();

    // Unrotate V by the given angle

    return v * cos(-angle) - u * sin(-angle);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindCompass 8th
 */
// ----------------------------------------------------------------------

TS::Value WindCompass8(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    std::vector<std::string> names{"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

    if (!q.param(kFmiWindDirection))
      return TS::None();

    NFmiMetTime t(ldt);
    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (value == kFloatMissing)
      return TS::None();

    int i = static_cast<int>((value + 22.5) / 45) % 8;
    return names.at(i);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindCompass 16th
 */
// ----------------------------------------------------------------------

TS::Value WindCompass16(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    std::vector<std::string> names{"N",
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
      return TS::None();

    NFmiMetTime t(ldt);
    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (value == kFloatMissing)
      return TS::None();

    int i = static_cast<int>((value + 11.25) / 22.5) % 16;
    return names.at(i);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindCompass 32th
 */
// ----------------------------------------------------------------------

TS::Value WindCompass32(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    std::vector<std::string> names{"N", "NbE", "NNE", "NEbN", "NE", "NEbE", "ENE", "EbN",
                                   "E", "EbS", "ESE", "SEbE", "SE", "SEbS", "SSE", "SbE",
                                   "S", "SbW", "SSW", "SWbS", "SW", "SWbW", "WSW", "WbS",
                                   "W", "WbN", "WNW", "NWbW", "NW", "NWbN", "NNW", "NbW"};

    if (!q.param(kFmiWindDirection))
      return TS::None();

    NFmiMetTime t(ldt);
    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (value == kFloatMissing)
      return TS::None();

    int i = static_cast<int>((value + 5.625) / 11.25) % 32;
    return names.at(i);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Cloudiness8th
 */
// ----------------------------------------------------------------------

TS::Value Cloudiness8th(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    if (!q.param(kFmiTotalCloudCover))
      return TS::None();

    NFmiMetTime t(ldt);
    float value = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (value == kFloatMissing)
      return TS::None();

    // This is the synoptic interpretation of 8s

    int n = boost::numeric_cast<int>(ceil(value / 12.5));
    return n;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WindChill
 */
// ----------------------------------------------------------------------

TS::Value WindChill(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    if (!q.param(kFmiWindSpeedMS))
      return TS::None();

    NFmiMetTime t(ldt);
    float wspd = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (!q.param(kFmiTemperature))
      return TS::None();

    float t2m = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (wspd == kFloatMissing || t2m == kFloatMissing)
      return TS::None();

    float chill = FmiWindChill(wspd, t2m);
    return chill;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief SummerSimmerIndex
 */
// ----------------------------------------------------------------------

TS::Value SummerSimmerIndex(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    if (!q.param(kFmiHumidity))
      return TS::None();

    NFmiMetTime t(ldt);

    float rh = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (!q.param(kFmiTemperature))
      return TS::None();

    float t2m = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (rh == kFloatMissing || t2m == kFloatMissing)
      return TS::None();

    float ssi = FmiSummerSimmerIndex(rh, t2m);
    return ssi;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief FeelsLike temperature
 */
// ----------------------------------------------------------------------

TS::Value FeelsLike(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    if (!q.param(kFmiHumidity))
      return TS::None();

    NFmiMetTime t(ldt);

    float rh = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (!q.param(kFmiWindSpeedMS))
      return TS::None();

    float wspd = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (!q.param(kFmiTemperature))
      return TS::None();

    float t2m = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (rh == kFloatMissing || t2m == kFloatMissing || wspd == kFloatMissing)
      return TS::None();

    // We permit radiation to be missing
    float rad = kFloatMissing;
    if (q.param(kFmiRadiationGlobal))
      rad = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    float ret = FmiFeelsLikeTemperature(wspd, rh, t2m, rad);

    if (ret == kFloatMissing)
      return TS::None();
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Apparent Temperature
 */
// ----------------------------------------------------------------------

TS::Value ApparentTemperature(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    if (!q.param(kFmiHumidity))
      return TS::None();

    NFmiMetTime t(ldt);

    float rh = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (!q.param(kFmiWindSpeedMS))
      return TS::None();

    float wspd = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (!q.param(kFmiTemperature))
      return TS::None();

    float t2m = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (rh == kFloatMissing || t2m == kFloatMissing || wspd == kFloatMissing)
      return TS::None();

    float ret = FmiApparentTemperature(wspd, rh, t2m);

    if (ret == kFloatMissing)
      return TS::None();
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Lower limit of water to snow conversion
 */
// ----------------------------------------------------------------------
TS::Value Snow1hLower(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    if (!q.param(kFmiPrecipitation1h))
      return TS::None();

    NFmiMetTime t(ldt);

    float prec1h = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    // FmiSnowLowerLimit fails if input is 'nan', check here.

    if (prec1h == kFloatMissing)
    {
      return TS::None();
    }
    float ret = FmiSnowLowerLimit(prec1h);
    if (ret == kFloatMissing)
      return TS::None();
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Upper limit of water to snow conversion
 */
// ----------------------------------------------------------------------
TS::Value Snow1hUpper(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    if (!q.param(kFmiPrecipitation1h))
      return TS::None();

    NFmiMetTime t(ldt);

    float prec1h = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    // FmiSnowUpperLimit fails if input is 'nan', check here.
    if (prec1h == kFloatMissing)
      return TS::None();

    float ret = FmiSnowUpperLimit(prec1h);
    if (ret == kFloatMissing)
      return TS::None();
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Snow estimate if no Snow1h parameter present
 */
// ----------------------------------------------------------------------
TS::Value Snow1h(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    // Use the actual Snow1h if it is present
    if (q.param(kFmiSnow1h))
      return q.param(kFmiSnow1h);

    if (!q.param(kFmiTemperature))
      return TS::None();

    NFmiMetTime t(ldt);

    float t2m = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (!q.param(kFmiWindSpeedMS))
      return TS::None();

    float wspd = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (!q.param(kFmiPrecipitation1h))
      return TS::None();

    float prec1h = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (t2m == kFloatMissing || wspd == kFloatMissing || prec1h == kFloatMissing)
      return TS::None();

    float snow1h = prec1h * FmiSnowWaterRatio(t2m, wspd);  // Can this be kFLoatMissing???
    return snow1h;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WeatherSymbol = WeatherSymbol3 + 100*Dark
 */
// ----------------------------------------------------------------------

TS::Value WeatherSymbol(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    if (!q.param(kFmiWeatherSymbol3))
      return TS::None();

    NFmiMetTime t(ldt);

    float symbol = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);
    if (symbol == kFloatMissing)
      return kFloatMissing;

    Fmi::Astronomy::solar_position_t sp =
        Fmi::Astronomy::solar_position(t, loc.longitude, loc.latitude);
    if (sp.dark())
      return 100 + symbol;
    return symbol;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Weather text
 */
// ----------------------------------------------------------------------

TS::Value WeatherText(QImpl &q,
                      const Spine::Location &loc,
                      const Fmi::LocalDateTime &ldt,
                      const std::string &lang,
                      const Spine::ParameterTranslations &translations)
{
  try
  {
    if (!q.param(kFmiWeatherSymbol3))
      return TS::None();

    NFmiMetTime t(ldt);

    float w = q.interpolate(NFmiPoint(loc.longitude, loc.latitude), t, maxgap);

    if (w == kFloatMissing)
      return TS::None();

    auto ret = translations.getTranslation("WeatherText", static_cast<int>(w), lang);
    if (!ret)
      return TS::None();

    return *ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate the smart weather symbol if possible
 */
// ----------------------------------------------------------------------

std::optional<int> calc_smart_symbol(QImpl &q,
                                     const NFmiPoint &latlon,
                                     const Fmi::LocalDateTime &ldt)
{
  try
  {
    // Cloudiness is almost always needed

    if (!q.param(kFmiTotalCloudCover))
      return {};

    NFmiMetTime t(ldt);

    const auto n = q.interpolate(latlon, t, maxgap);

    if (n == kFloatMissing)
      return {};

    // The first parameter we need always is POT. We allow it to be missing though.

    if (q.param(kFmiProbabilityThunderstorm))
    {
      const auto thunder = q.interpolate(latlon, t, maxgap);

      if (thunder >= thunder_limit1 && thunder != kFloatMissing)
      {
        int nclass = (n < cloud_limit6 ? 0 : (n < cloud_limit8 ? 1 : 2));
        return 71 + 3 * nclass;  // 71,74,77
      }
    }

    // No thunder (or not available). Then we always need precipitation rate

    if (!q.param(kFmiPrecipitation1h))
      return {};

    const auto rain = q.interpolate(latlon, t, maxgap);

    if (rain == kFloatMissing)
      return {};

    if (rain < rain_limit1)
    {
      // No precipitation. Now we need only fog/cloudiness

      if (q.param(kFmiFogIntensity))
      {
        const auto fog = q.interpolate(latlon, t, maxgap);
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
    if (q.param(kFmiPotentialPrecipitationForm) || q.param(kFmiPrecipitationForm))
      rform = static_cast<int>(q.interpolate(latlon, t, maxgap));

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
      if (q.param(kFmiPotentialPrecipitationType) || q.param(kFmiPrecipitationType))
        rtype = static_cast<int>(q.interpolate(latlon, t, maxgap));

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate the weather number used as basis for SmartSymbol
 */
// ----------------------------------------------------------------------

std::optional<int> calc_weather_number(QImpl &q,
                                       const NFmiPoint &latlon,
                                       const Fmi::LocalDateTime &ldt)
{
  try
  {
    NFmiMetTime t(ldt);

    // Cloudiness is optional
    float n = kFloatMissing;
    if (q.param(kFmiTotalCloudCover))
      n = q.interpolate(latlon, t, maxgap);

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
      rain = q.interpolate(latlon, t, maxgap);

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
    if (q.param(kFmiPotentialPrecipitationForm) || q.param(kFmiPrecipitationForm))
      rform = q.interpolate(latlon, t, maxgap);

    int rform_class = (rform == kFloatMissing ? 9 : static_cast<int>(rform));

    // Precipitation type is optional
    float rtype = kFloatMissing;
    if (q.param(kFmiPotentialPrecipitationType) || q.param(kFmiPrecipitationType))
      rtype = q.interpolate(latlon, t, maxgap);

    int rtype_class = (rtype == kFloatMissing ? 9 : static_cast<int>(rtype));

    // Thunder is optional
    float thunder = kFloatMissing;
    if (q.param(kFmiProbabilityThunderstorm))
      thunder = q.interpolate(latlon, t, maxgap);

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
      fog = q.interpolate(latlon, t, maxgap);

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}  // namespace Querydata

// ----------------------------------------------------------------------
/*!
 * \brief SmartSymbol
 */
// ----------------------------------------------------------------------

TS::Value SmartSymbolNumber(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    NFmiPoint latlon(loc.longitude, loc.latitude);

    auto symbol = calc_smart_symbol(q, latlon, ldt);

    if (!symbol || *symbol == kFloatMissing)
      return TS::None();

    // Add day/night information
    Fmi::Astronomy::solar_position_t sp =
        Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);

    if (sp.dark())
      return 100 + *symbol;
    return *symbol;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief WeatherNumber
 */
// ----------------------------------------------------------------------

TS::Value WeatherNumber(QImpl &q, const Spine::Location &loc, const Fmi::LocalDateTime &ldt)
{
  try
  {
    NFmiPoint latlon(loc.longitude, loc.latitude);

    auto number = calc_weather_number(q, latlon, ldt);

    if (!number)
      return TS::None();

    return *number;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Symbol text
 */
// ----------------------------------------------------------------------

TS::Value SmartSymbolText(QImpl &q,
                          const Spine::Location &loc,
                          const Fmi::LocalDateTime &ldt,
                          const std::string &lang,
                          const Spine::ParameterTranslations &translations)
{
  try
  {
    NFmiPoint latlon(loc.longitude, loc.latitude);

    auto symbol = calc_smart_symbol(q, latlon, ldt);

    if (!symbol)
      return TS::None();

    auto ret = translations.getTranslation("SmartSymbolText", *symbol, lang);

    if (!ret)
      return TS::None();

    return *ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Grid north deviation
 */
// ----------------------------------------------------------------------

TS::Value GridNorth(const QImpl &q, const Spine::Location &loc)
{
  try
  {
    Fmi::CoordinateTransformation transformation("WGS84", q.SpatialReference());
    auto opt_angle = Fmi::OGR::gridNorth(transformation, loc.longitude, loc.latitude);
    if (!opt_angle)
      return TS::None();
    return *opt_angle;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

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

QImpl::QImpl(const SharedModel &theModel)
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
      throw Fmi::Exception(BCP, "Cannot initialize any empty view over multiple models");

    for (auto &model : itsModels)
      itsInfos.push_back(model->info());

    if (itsInfos.size() > 1)
      itsInfo = std::make_shared<NFmiMultiQueryInfo>(itsInfos);
    else
      itsInfo = itsInfos[0];

    // Establish hash value
    itsHashValue = 0;
    for (const auto &model : itsModels)
    {
      Fmi::hash_combine(itsHashValue, Fmi::hash_value(model));
    }

    // Establish unique valid times
    std::set<Fmi::DateTime> uniquetimes;
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Direct access to the data
 */
// ----------------------------------------------------------------------

std::shared_ptr<NFmiFastQueryInfo> QImpl::info()
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
      meta.firstTime = Fmi::DateTime::NOT_A_DATE_TIME;

    // Get querydata last time
    if (qi.LastTime())
      meta.lastTime = qi.ValidTime();
    else
      meta.lastTime = Fmi::DateTime::NOT_A_DATE_TIME;

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

    // Get querydata validtimes
    std::list<Fmi::DateTime> times;
    qi.ResetTime();
    while (qi.NextTime())
    {
      times.emplace_back(qi.ValidTime());
    }
    meta.times = times;

    // Get querydata timesteps size
    meta.nTimeSteps = qi.SizeTimes();

    // Get the parameter list from querydatainfo
    std::list<ModelParameter> params;
    for (qi.ResetParam(); qi.NextParam(false);)
    {
      const int paramID = boost::numeric_cast<int>(qi.Param().GetParamIdent());
      const std::string paramName = TimeSeries::ParameterFactory::instance().name(paramID);
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
      const NFmiLevel &lev = *qi.Level();

      const auto *type = level_name(lev.LevelType());
      const auto *name = lev.GetName().CharPtr();
      levels.emplace_back(type, name, lev.LevelValue());
    }

    meta.levels = levels;
    meta.parameters = params;

    // Point data does have an envelope
    meta.wgs84Envelope = *(WGS84EnvelopeFactory::Get(itsModels[0]->info()));

    // Get projection string
    if (qi.Area() == nullptr)
    {
      meta.WKT = "nan";
      return meta;
    }

    meta.WKT = qi.Area()->WKT();

    // Get querydata area info

    const NFmiArea *a = qi.Area();

    meta.ullon = a->TopLeftLatLon().X();
    meta.ullat = a->TopLeftLatLon().Y();
    meta.urlon = a->TopRightLatLon().X();
    meta.urlat = a->TopRightLatLon().Y();
    meta.bllon = a->BottomLeftLatLon().X();
    meta.bllat = a->BottomLeftLatLon().Y();
    meta.brlon = a->BottomRightLatLon().X();
    meta.brlat = a->BottomRightLatLon().Y();
    meta.clon = a->CenterLatLon().X();
    meta.clat = a->CenterLatLon().Y();

    meta.areaWidth = a->WorldXYWidth() / 1000.0;
    meta.areaHeight = a->WorldXYHeight() / 1000.0;

    meta.aspectRatio = a->WorldXYAspectRatio();

    // Get querydata grid info

    const NFmiGrid *g = qi.Grid();
    meta.xNumber = boost::numeric_cast<unsigned int>(g->XNumber());
    meta.yNumber = boost::numeric_cast<unsigned int>(g->YNumber());

    meta.xResolution = a->WorldXYWidth() / (g->XNumber() - 1) / 1000.0;
    meta.yResolution = a->WorldXYHeight() / (g->YNumber() - 1) / 1000.0;

    return meta;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return modification time of the model
 */
// ----------------------------------------------------------------------

Fmi::DateTime QImpl::modificationTime() const
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return estimated expiration time of the model
 */
// ----------------------------------------------------------------------

Fmi::DateTime QImpl::expirationTime() const
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return valid times of the model
 */
// ----------------------------------------------------------------------

std::shared_ptr<ValidTimeList> QImpl::validTimes() const
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void QImpl::setParameterTranslations(
    const std::shared_ptr<Spine::ParameterTranslations> &translations)
{
  if (!translations)
    throw Fmi::Exception(BCP, "empty std::shared_ptr<>");
  itsParameterTranslations = translations;
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
 * \brief Return the spatial reference
 */
// ----------------------------------------------------------------------

const Fmi::SpatialReference &QImpl::SpatialReference() const
{
  return itsInfo->SpatialReference();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return data coordinates
 */
// ----------------------------------------------------------------------

Fmi::CoordinateMatrix QImpl::CoordinateMatrix() const
{
  return itsInfo->CoordinateMatrix(false);
}

// ----------------------------------------------------------------------
/*!
 * \brief Return data coordinates with possible wraparound column for global data
 */
// ----------------------------------------------------------------------

Fmi::CoordinateMatrix QImpl::FullCoordinateMatrix() const
{
  return itsInfo->CoordinateMatrix(true);
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    const auto *a = itsInfo->Area();
    if (a == nullptr)
      throw Fmi::Exception(BCP, "Attempt to access unset area in querydata");
    return *a;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
      throw Fmi::Exception(BCP, "Attempt to access unset grid in querydata");
    return *itsInfo->Grid();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    auto level = itsInfo->Level();
    if (!level)
      throw Fmi::Exception(BCP, "INTERNAL ERROR: Level not available");
    return level->LevelValue();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Matrix calculation of derived values
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::calculatedValues(const Spine::Parameter &theParam,
                                              const Fmi::DateTime &theInterpolatedTime)
{
  try
  {
    const auto *grid = itsInfo->Grid();
    if (grid == nullptr)
      throw Fmi::Exception(BCP, "Cannot extract grid of values from point data");
    const auto nx = grid->XNumber();
    const auto ny = grid->YNumber();

    NFmiDataMatrix<float> ret(nx, ny, kFloatMissing);

    switch (theParam.number())
    {
      case kFmiWindChill:
      {
        if (param(kFmiWindSpeedMS) && param(kFmiTemperature))
        {
          auto t2m = values(theInterpolatedTime);
          param(kFmiWindSpeedMS);
          auto wspd = values(theInterpolatedTime);
          for (std::size_t j = 0; j < t2m.NY(); ++j)
            for (std::size_t i = 0; i < t2m.NX(); ++i)
              ret[i][j] = FmiWindChill(wspd[i][j], t2m[i][j]);
        }
        break;
      }
      case kFmiSummerSimmerIndex:
      {
        if (param(kFmiHumidity) && param(kFmiTemperature))
        {
          auto t2m = values(theInterpolatedTime);
          param(kFmiHumidity);
          auto rh = values(theInterpolatedTime);
          for (std::size_t j = 0; j < t2m.NY(); ++j)
            for (std::size_t i = 0; i < t2m.NX(); ++i)
              ret[i][j] = FmiSummerSimmerIndex(rh[i][j], t2m[i][j]);
        }
        break;
      }
      case kFmiFeelsLike:
      {
        if (param(kFmiHumidity) && param(kFmiWindSpeedMS) && param(kFmiTemperature))
        {
          auto t2m = values(theInterpolatedTime);
          param(kFmiHumidity);
          auto rh = values(theInterpolatedTime);
          param(kFmiWindSpeedMS);
          auto wpsd = values(theInterpolatedTime);

          bool has_radiation = param(kFmiRadiationGlobal);
          if (has_radiation)
            ret = values(theInterpolatedTime);  // Using ret as temporary storage for radiation
          for (std::size_t j = 0; j < t2m.NY(); ++j)
            for (std::size_t i = 0; i < t2m.NX(); ++i)
              if (has_radiation)
                ret[i][j] = FmiFeelsLikeTemperature(wpsd[i][j], rh[i][j], t2m[i][j], ret[i][j]);
        }
        break;
      }
      case kFmiApparentTemperature:
      {
        if (param(kFmiHumidity) && param(kFmiWindSpeedMS) && param(kFmiTemperature))
        {
          auto t2m = values(theInterpolatedTime);
          param(kFmiHumidity);
          auto rh = values(theInterpolatedTime);
          param(kFmiWindSpeedMS);
          auto wpsd = values(theInterpolatedTime);
          for (std::size_t j = 0; j < t2m.NY(); ++j)
            for (std::size_t i = 0; i < t2m.NX(); ++i)
              ret[i][j] = FmiApparentTemperature(wpsd[i][j], rh[i][j], t2m[i][j]);
        }
        break;
      }
      default:
      {
        throw Fmi::Exception(BCP, "Unable to fetch parameter as a value matrix")
            .addParameter("parameter", theParam.name());
      }
    }
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract calculated values from querydata");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values at grid points
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::values()
{
  try
  {
    return itsInfo->Values();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract time interpolated values at grid points
 * \param theInterpolatedTime The desired time
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::values(const NFmiMetTime &theInterpolatedTime)
{
  try
  {
    return itsInfo->Values(theInterpolatedTime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract time interpolated values at grid points
 * \param theInterpolatedTime The desired time
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::values(const Spine::Parameter &theParam,
                                    const Fmi::DateTime &theInterpolatedTime)
{
  try
  {
    switch (theParam.type())
    {
      case Spine::Parameter::Type::Data:
      {
        if (!param(theParam.number()))
          throw Fmi::Exception(BCP,
                               "Parameter " + theParam.name() + " is not available in the data");
        return values(theInterpolatedTime);
      }
      case Spine::Parameter::Type::DataDerived:
      case Spine::Parameter::Type::DataIndependent:
      default:
      {
        return calculatedValues(theParam, theInterpolatedTime);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Interpolate values
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::values(const Fmi::CoordinateMatrix &theLatlonMatrix,
                                    const NFmiMetTime &theTime,
                                    float P,
                                    float H)
{
  try
  {
    return itsInfo->Values(theLatlonMatrix, theTime, P, H);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract subgrid values
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::croppedValues(int x1, int y1, int x2, int y2) const
{
  try
  {
    return itsInfo->CroppedValues(x1, y1, x2, y2);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values for pressure level
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::pressureValues(const NFmiMetTime &theInterpolatedTime,
                                            float wantedPressureLevel)
{
  try
  {
    return itsInfo->PressureValues(theInterpolatedTime, wantedPressureLevel);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values for pressure level
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::pressureValues(const NFmiGrid &theWantedGrid,
                                            const NFmiMetTime &theInterpolatedTime,
                                            float wantedPressureLevel)
{
  try
  {
    return itsInfo->PressureValues(theWantedGrid, theInterpolatedTime, wantedPressureLevel);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

NFmiDataMatrix<float> QImpl::pressureValues(const NFmiGrid &theWantedGrid,
                                            const NFmiMetTime &theInterpolatedTime,
                                            float wantedPressureLevel,
                                            bool relative_uv)
{
  try
  {
    return itsInfo->PressureValues(
        theWantedGrid, theInterpolatedTime, wantedPressureLevel, relative_uv);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values for grid
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::gridValues(const NFmiGrid &theWantedGrid,
                                        const NFmiMetTime &theInterpolatedTime,
                                        bool relative_uv)
{
  try
  {
    return itsInfo->GridValues(theWantedGrid, theInterpolatedTime, relative_uv);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract values for height level
 */
// ----------------------------------------------------------------------

NFmiDataMatrix<float> QImpl::heightValues(const NFmiGrid &theWantedGrid,
                                          const NFmiMetTime &theInterpolatedTime,
                                          float wantedHeightLevel,
                                          bool relative_uv)
{
  try
  {
    return itsInfo->HeightValues(
        theWantedGrid, theInterpolatedTime, wantedHeightLevel, relative_uv);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract data value
 */
// ----------------------------------------------------------------------

TS::Value QImpl::dataValue(const ParameterOptions &opt,
                           const NFmiPoint &latlon,
                           const Fmi::LocalDateTime &ldt)
{
  NFmiMetTime t = ldt;

  // Change the year if the data contains climatology
  if (isClimatology())
  {
    int year = originTime().PosixTime().date().year();
    t.SetYear(boost::numeric_cast<short>(year));

    // Climatology data might not be for a leap year
    if (t.GetMonth() == 2 && t.GetDay() == 29 && !is_leap_year(year))
      t.SetDay(28);
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
    return TS::None();

  return interpolatedValue;
}

TS::Value QImpl::dataValueAtPressure(const ParameterOptions &opt,
                                     const NFmiPoint &latlon,
                                     const Fmi::LocalDateTime &ldt,
                                     float pressure)
{
  TS::Value retval = TS::None();

  NFmiMetTime t = ldt;

  float interpolatedValue = interpolateAtPressure(latlon, t, pressure, maxgap);

  // If we got no value and the proper flag is on,
  // find the nearest point with valid values and use
  // the values from that point

  if (interpolatedValue == kFloatMissing && opt.findnearestvalidpoint)
    interpolatedValue = interpolateAtPressure(opt.nearestpoint, t, pressure, maxgap);

  if (interpolatedValue != kFloatMissing)
    retval = interpolatedValue;

  return retval;
}

TS::Value QImpl::dataValueAtHeight(const ParameterOptions &opt,
                                   const NFmiPoint &latlon,
                                   const Fmi::LocalDateTime &ldt,
                                   float height)
{
  TS::Value retval = TS::None();

  NFmiMetTime t = ldt;

  float interpolatedValue = interpolateAtHeight(latlon, t, height, maxgap);

  // If we got no value and the proper flag is on,
  // find the nearest point with valid values and use
  // the values from that point

  if (interpolatedValue == kFloatMissing && opt.findnearestvalidpoint)
    interpolatedValue = interpolateAtHeight(opt.nearestpoint, t, height, maxgap);

  if (interpolatedValue != kFloatMissing)
    retval = interpolatedValue;

  return retval;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract data independent parameter value
 */
// ----------------------------------------------------------------------

TS::Value QImpl::dataIndependentValue(const ParameterOptions &opt,
                                      const Fmi::LocalDateTime &ldt,
                                      double levelResult)
{
  // Some shorthand variables
  const std::string &pname = opt.par.name();
  const Spine::Location &loc = opt.loc;

  switch (opt.par.number())
  {
    case kFmiPlace:
      return opt.place;
    case kFmiName:
      return loc.name;
    case kFmiISO2:
      return loc.iso2;
    case kFmiGEOID:
    {
      if (loc.geoid == 0)  // not sure why this is still here
        return TS::None();
      return Fmi::to_string(loc.geoid);
    }
    case kFmiLatitude:
      return loc.latitude;
    case kFmiLongitude:
      return loc.longitude;
    case kFmiLatLon:
    case kFmiLonLat:
      return TS::LonLat(loc.longitude, loc.latitude);
    case kFmiRegion:
    {
      // This reintroduces an older bug/feature where the name of the location is given as a
      // region if it doesn't belong to any administrative region. (i.e. Helsinki doesn't have
      // region, Kumpula has.) Also checking whether the loc.name has valid data, if it's empty as
      // well - which shoudn't occur - we return nan

      if (!loc.area.empty())
        return loc.area;  // Administrative region known.

      if (loc.name.empty())
        // No area (administrative region) nor name known.
        return TS::None();

      // Place name known, administrative region unknown.
      return loc.name;
    }
    case kFmiCountry:
      return opt.country;
    case kFmiFeature:
      return loc.feature;
    case kFmiTZ:
    {
      if (ldt.zone())
        // return ldt.zone()->std_zone_name(); // Not present in new Date library
        return ldt.abbrev();
      return TS::None();
    }
    case kFmiLocalTZ:
      return loc.timezone;
    case kFmiLevel:
      return levelResult;
    case kFmiNearLatitude:
      return opt.lastpoint.Y();
    case kFmiNearLongitude:
      return opt.lastpoint.X();
    case kFmiNearLatLon:
    case kFmiNearLonLat:
      return TS::LonLat(opt.lastpoint.X(), opt.lastpoint.Y());
    case kFmiPopulation:
      return Fmi::to_string(loc.population);
    case kFmiElevation:
      return Fmi::to_string(loc.elevation);
    case kFmiDEM:
      return Fmi::to_string(loc.dem);
    case kFmiCoverType:
      return Fmi::to_string(static_cast<int>(loc.covertype));
    case kFmiModel:
      return opt.producer;
    case kFmiTime:
      return opt.timeformatter.format(ldt);
    case kFmiISOTime:
      return Fmi::to_iso_string(ldt.local_time());
    case kFmiXMLTime:
      return Fmi::to_iso_extended_string(ldt.local_time());
    case kFmiLocalTime:
    {
      auto localtz = Fmi::TimeZoneFactory::instance().time_zone_from_string(loc.timezone);
      Fmi::DateTime utc = ldt.utc_time();
      Fmi::LocalDateTime localt(utc, localtz);
      return opt.timeformatter.format(localt);
    }
    case kFmiUTCTime:
      return opt.timeformatter.format(ldt.utc_time());
    case kFmiEpochTime:
    {
      Fmi::DateTime time_t_epoch(Fmi::Date(1970, 1, 1));
      Fmi::TimeDuration diff = ldt.utc_time() - time_t_epoch;
      return Fmi::to_string(diff.total_seconds());
    }
    case kFmiOriginTime:
    {
      if (!time(ldt.utc_time()))
      {
        // Search first valid time after the desired time, and choose that origintime
        bool ok = false;
        for (resetTime(); !ok && nextTime();)
          ok = (Fmi::DateTime(validTime()) > ldt.utc_time());
        if (!ok)
          return TS::None();
      }
      Fmi::DateTime utc = originTime();
      Fmi::LocalDateTime localt(utc, ldt.zone());
      return opt.timeformatter.format(localt);
    }
    case kFmiModTime:
    {
      Fmi::DateTime utc = modificationTime();
      Fmi::LocalDateTime localt(utc, ldt.zone());
      return opt.timeformatter.format(localt);
    }
    case kFmiDark:
    {
      auto pos = Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(pos.dark()));
    }
    case kFmiMoonPhase:
      return Fmi::Astronomy::moonphase(ldt.utc_time());
    case kFmiMoonrise:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      return opt.timeformatter.format(ltime.moonrise.local_time());
    }
    case kFmiMoonrise2:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);

      if (ltime.moonrise2_today())
        return opt.timeformatter.format(ltime.moonrise2.local_time());

      return std::string("");
    }
    case kFmiMoonset:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      return opt.timeformatter.format(ltime.moonset.local_time());
    }
    case kFmiMoonset2:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      if (ltime.moonset2_today())
        return opt.timeformatter.format(ltime.moonset2.local_time());
      return std::string("");
    }
    case kFmiMoonriseToday:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(ltime.moonrise_today()));
    }
    case kFmiMoonrise2Today:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(ltime.moonrise2_today()));
    }
    case kFmiMoonsetToday:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(ltime.moonset_today()));
    }
    case kFmiMoonset2Today:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(ltime.moonset2_today()));
    }
    case kFmiMoonUp24h:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(ltime.above_horizont_24h()));
    }
    case kFmiMoonDown24h:
    {
      auto ltime = Fmi::Astronomy::lunar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(!ltime.moonrise_today() && !ltime.moonset_today() &&
                                             !ltime.above_horizont_24h()));
    }
    case kFmiSunrise:
    {
      auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      return opt.timeformatter.format(stime.sunrise.local_time());
    }
    case kFmiSunset:
    {
      auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      return opt.timeformatter.format(stime.sunset.local_time());
    }
    case kFmiNoon:
    {
      auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_iso_string(stime.noon.local_time());
    }
    case kFmiSunriseToday:
    {
      auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(stime.sunrise_today()));
    }
    case kFmiSunsetToday:
    {
      auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      return Fmi::to_string(static_cast<int>(stime.sunset_today()));
    }
    case kFmiDayLength:
    {
      auto stime = Fmi::Astronomy::solar_time(ldt, loc.longitude, loc.latitude);
      auto seconds = stime.daylength().total_seconds();
      auto minutes = lround(seconds / 60.0);
      return Fmi::to_string(minutes);
    }
    case kFmiTimeString:
      return format_date(ldt, opt.outlocale, opt.timestring);
    case kFmiWDay:
      return format_date(ldt, opt.outlocale, "%a");
    case kFmiWeekday:
      return format_date(ldt, opt.outlocale, "%A");
    case kFmiMon:
      return format_date(ldt, opt.outlocale, "%b");
    case kFmiMonth:
      return format_date(ldt, opt.outlocale, "%B");
    case kFmiSunElevation:
    {
      auto pos = Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      return pos.elevation;
    }
    case kFmiSunDeclination:
    {
      auto pos = Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      return pos.declination;
    }
    case kFmiSunAzimuth:
    {
      auto pos = Fmi::Astronomy::solar_position(ldt, loc.longitude, loc.latitude);
      return pos.azimuth;
    }
    case kFmiGridNorth:
      return GridNorth(*this, loc);
    case kFmiHour:
      return Fmi::to_string(ldt.local_time().time_of_day().hours());

      // The following parameters are added for for obsengine compability reasons
      // so that we can have e.g. fmisid identifier for observations in query which
      // has both observations and forecasts.
      // Later on support was added for pointwise querydata.

    case kFmiStationLongitude:
    {
      if (loc.fmisid)
        return loc.longitude;
      if (!isGrid())
        return latLon().X();
      return TS::None();
    }
    case kFmiStationLatitude:
    {
      if (loc.fmisid)
        return loc.latitude;
      if (!isGrid())
        return latLon().Y();
      return TS::None();
    }
    case kFmiStationName:
    {
      if (isGrid())
        return TS::None();
      return info()->Location()->GetName().CharPtr();
    }
    case kFmiFMISID:
    {
      if (loc.fmisid)
        return *loc.fmisid;
      if (!isGrid())
        return info()->Location()->GetIdent();
      return TS::None();
    }
    case kFmiWmoStationNumber:
    case kFmiLPNN:
    case kFmiRWSID:
    {
      if (!isGrid())
        return info()->Location()->GetIdent();
      return TS::None();
    }

    case kFmiDistance:
    {
      if (isGrid())
        return TS::None();
      return info()->Location()->Distance(NFmiPoint(loc.longitude, loc.latitude));
    }
    case kFmiDirection:
    {
      if (isGrid())
        return TS::None();
      auto dir = info()->Location()->Direction(NFmiPoint(loc.longitude, loc.latitude));
      if (dir < 0)
        dir += 360;
      return dir;
    }
    case kFmiStationType:
    case kFmiStationary:
    case kFmiSensorNo:
      return TS::None();
    default:
      break;
  }

  if (pname.substr(0, 5) == "date(" && pname[pname.size() - 1] == ')')
    return format_date(ldt, opt.outlocale, pname.substr(5, pname.size() - 6));

  throw Fmi::Exception(BCP,
                       "Unknown DataIndependent special function '" + pname + "' with number " +
                           Fmi::to_string(opt.par.number()));
}

// ======================================================================

TS::Value QImpl::value(const ParameterOptions &opt, const Fmi::LocalDateTime &ldt)
{
  try
  {
    // Default return value
    TS::Value retval = TS::None();

    // Shorthand variables
    const Spine::Location &loc = opt.loc;

    // Update last accessed point.

    NFmiPoint latlon(loc.longitude, loc.latitude);

    switch (opt.par.type())
    {
      case Spine::Parameter::Type::Data:
      {
        opt.lastpoint = latlon;
        if (param(opt.par.number()))
          retval = dataValue(opt, latlon, ldt);
        break;
      }
      case Spine::Parameter::Type::DataDerived:
      {
        switch (opt.par.number())
        {
          case kFmiLatitude:
          {
            retval = loc.latitude;
            break;
          }
          case kFmiLongitude:
          {
            retval = loc.longitude;
            break;
          }
          case kFmiLatLon:
          case kFmiLonLat:
          {
            retval = TS::LonLat(loc.longitude, loc.latitude);
            break;
          }
          case kFmiWindCompass8:
          {
            retval = WindCompass8(*this, loc, ldt);
            break;
          }
          case kFmiWindCompass16:
          {
            retval = WindCompass16(*this, loc, ldt);
            break;
          }
          case kFmiWindCompass32:
          {
            retval = WindCompass32(*this, loc, ldt);
            break;
          }
          case kFmiCloudiness8th:
          {
            retval = Cloudiness8th(*this, loc, ldt);
            break;
          }
          case kFmiWindChill:
          {
            retval = WindChill(*this, loc, ldt);
            break;
          }
          case kFmiSummerSimmerIndex:
          {
            retval = SummerSimmerIndex(*this, loc, ldt);
            break;
          }
          case kFmiFeelsLike:
          {
            retval = FeelsLike(*this, loc, ldt);
            break;
          }
          case kFmiApparentTemperature:
          {
            retval = ApparentTemperature(*this, loc, ldt);
            break;
          }
          case kFmiWeather:
          {
            retval = WeatherText(*this, loc, ldt, opt.language, *itsParameterTranslations);
            break;
          }
          case kFmiWeatherSymbol:
          {
            retval = WeatherSymbol(*this, loc, ldt);
            break;
          }
          case kFmiSmartSymbol:
          {
            retval = SmartSymbolNumber(*this, loc, ldt);
            break;
          }
          case kFmiSmartSymbolText:
          {
            retval = SmartSymbolText(*this, loc, ldt, opt.language, *itsParameterTranslations);
            break;
          }
          case kFmiWeatherNumber:
          {
            retval = WeatherNumber(*this, loc, ldt);
            break;
          }
          case kFmiSnow1hLower:
          {
            retval = Snow1hLower(*this, loc, ldt);
            break;
          }
          case kFmiSnow1hUpper:
          {
            retval = Snow1hUpper(*this, loc, ldt);
            break;
          }
          case kFmiSnow1h:
          {
            retval = Snow1h(*this, loc, ldt);
            break;
          }
          case kFmiWindUMS:
          {
            if (isRelativeUV())
              retval = WindUMS(*this, loc, ldt);
            else if (param(kFmiWindUMS))
              retval = dataValue(opt, latlon, ldt);
            break;
          }
          case kFmiWindVMS:
          {
            if (isRelativeUV())
              retval = WindVMS(*this, loc, ldt);
            else if (param(kFmiWindVMS))
              retval = dataValue(opt, latlon, ldt);
            break;
          }
          default:
            throw Fmi::Exception(BCP, "Unknown DataDerived parameter '" + opt.par.name() + "'!");
        }
        break;
      }
      case Spine::Parameter::Type::DataIndependent:
      {
        retval = dataIndependentValue(opt, ldt, levelValue());
        break;
      }
    }

    if (const auto *ptr = std::get_if<double>(&retval))
    {
      if (*ptr == kFloatMissing)
        retval = TS::None();
    }

    return retval;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::Value QImpl::valueAtPressure(const ParameterOptions &opt,
                                 const Fmi::LocalDateTime &ldt,
                                 float pressure)
{
  try
  {
    // Default return value
    TS::Value retval = TS::None();

    // Some shorthand variables
    const Spine::Location &loc = opt.loc;

    // Update last accessed point.

    NFmiPoint latlon(loc.longitude, loc.latitude);

    switch (opt.par.type())
    {
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
            retval = TS::None();
          else
            retval = interpolatedValue;
        }

        break;
      }
      case Spine::Parameter::Type::DataDerived:
      {
        auto num = opt.par.number();
        if (num == kFmiLatitude)
          retval = loc.latitude;
        else if (num == kFmiLongitude)
          retval = loc.longitude;
        else if (num == kFmiLatLon || num == kFmiLonLat)
          retval = TS::LonLat(loc.longitude, loc.latitude);
        else if (num == kFmiWindUMS || num == kFmiWindVMS)
        {
          if (param(opt.par.number()) && (itsModels[0]->levelName() != "surface") &&
              !isClimatology())
          {
            if (isRelativeUV())
              retval = (num == kFmiWindUMS
                            ? WindUMS(*this, loc, ldt, pressure, InterpolationMethod::PRESSURE)
                            : WindVMS(*this, loc, ldt, pressure, InterpolationMethod::PRESSURE));
            else
              retval = dataValueAtPressure(opt, latlon, ldt, pressure);
          }
        }
        break;
      }
      case Spine::Parameter::Type::DataIndependent:
      {
        retval = dataIndependentValue(opt, ldt, pressure);
        break;
      }
    }

    if (const auto *ptr = std::get_if<double>(&retval))
    {
      if (*ptr == kFloatMissing)
        retval = TS::None();
    }

    return retval;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TS::Value QImpl::valueAtHeight(const ParameterOptions &opt,
                               const Fmi::LocalDateTime &ldt,
                               float height)
{
  try
  {
    // Default return value
    TS::Value retval = TS::None();

    // Some shorthand variables
    const Spine::Location &loc = opt.loc;

    // Update last accessed point.

    NFmiPoint latlon(loc.longitude, loc.latitude);

    switch (opt.par.type())
    {
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
            retval = TS::None();
          else
            retval = interpolatedValue;
        }

        break;
      }
      case Spine::Parameter::Type::DataDerived:
      {
        auto num = opt.par.number();
        if (num == kFmiLatitude)
          retval = loc.latitude;
        else if (num == kFmiLongitude)
          retval = loc.longitude;
        else if (num == kFmiLatLon || num == kFmiLonLat)
          retval = TS::LonLat(loc.longitude, loc.latitude);
        else if (num == kFmiWindUMS || num == kFmiWindVMS)
        {
          if (param(opt.par.number()) && (itsModels[0]->levelName() != "surface") &&
              !isClimatology())
          {
            if (isRelativeUV())
              retval = (num == kFmiWindUMS
                            ? WindUMS(*this, loc, ldt, height, InterpolationMethod::HEIGHT)
                            : WindVMS(*this, loc, ldt, height, InterpolationMethod::HEIGHT));
            else
              retval = dataValueAtHeight(opt, latlon, ldt, height);
          }
        }

        break;
      }
      case Spine::Parameter::Type::DataIndependent:
      {
        retval = dataIndependentValue(opt, ldt, height);
        break;
      }
    }

    if (const auto *ptr = std::get_if<double>(&retval))
    {
      if (*ptr == kFloatMissing)
        retval = TS::None();
    }

    return retval;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// one location, many timesteps
TS::TimeSeriesPtr QImpl::values(const ParameterOptions &param,
                                const TS::TimeSeriesGenerator::LocalTimeList &tlist)
{
  try
  {
    TS::TimeSeriesPtr ret(new TS::TimeSeries);

    for (const Fmi::LocalDateTime &ldt : tlist)
    {
      ret->emplace_back(TS::TimedValue(ldt, value(param, ldt)));
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
TS::TimeSeriesPtr QImpl::valuesAtPressure(const ParameterOptions &param,
                                          const TS::TimeSeriesGenerator::LocalTimeList &tlist,
                                          float pressure)
{
  try
  {
    TS::TimeSeriesPtr ret(new TS::TimeSeries);

    for (const Fmi::LocalDateTime &ldt : tlist)
    {
      ret->emplace_back(TS::TimedValue(ldt, valueAtPressure(param, ldt, pressure)));
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
TS::TimeSeriesPtr QImpl::valuesAtHeight(const ParameterOptions &param,
                                        const TS::TimeSeriesGenerator::LocalTimeList &tlist,
                                        float height)
{
  try
  {
    TS::TimeSeriesPtr ret(new TS::TimeSeries);

    for (const Fmi::LocalDateTime &ldt : tlist)
    {
      ret->emplace_back(TS::TimedValue(ldt, valueAtHeight(param, ldt, height)));
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// many locations (indexmask), many timesteps
TS::TimeSeriesGroupPtr QImpl::values(const ParameterOptions &param,
                                     const NFmiIndexMask &indexmask,
                                     const TS::TimeSeriesGenerator::LocalTimeList &tlist)
{
  try
  {
    TS::TimeSeriesGroupPtr ret(new TS::TimeSeriesGroup);

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

      TS::TimeSeriesPtr timeseries = values(paramOptions, tlist);
      TS::LonLat lonlat(latlon.X(), latlon.Y());

      ret->emplace_back(lonlat, *timeseries);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
TS::TimeSeriesGroupPtr QImpl::valuesAtPressure(const ParameterOptions &param,
                                               const NFmiIndexMask &indexmask,
                                               const TS::TimeSeriesGenerator::LocalTimeList &tlist,
                                               float pressure)
{
  try
  {
    TS::TimeSeriesGroupPtr ret(new TS::TimeSeriesGroup);

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

      TS::TimeSeriesPtr timeseries = valuesAtPressure(paramOptions, tlist, pressure);
      TS::LonLat lonlat(latlon.X(), latlon.Y());

      ret->emplace_back(lonlat, *timeseries);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
TS::TimeSeriesGroupPtr QImpl::valuesAtHeight(const ParameterOptions &param,
                                             const NFmiIndexMask &indexmask,
                                             const TS::TimeSeriesGenerator::LocalTimeList &tlist,
                                             float height)
{
  try
  {
    TS::TimeSeriesGroupPtr ret(new TS::TimeSeriesGroup);

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

      TS::TimeSeriesPtr timeseries = valuesAtHeight(paramOptions, tlist, height);
      TS::LonLat lonlat(latlon.X(), latlon.Y());

      ret->emplace_back(lonlat, *timeseries);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// many locations (llist), many timesteps

// BUG?? Why is maxdistance in the API?

TS::TimeSeriesGroupPtr QImpl::values(const ParameterOptions &param,
                                     const Spine::LocationList &llist,
                                     const TS::TimeSeriesGenerator::LocalTimeList &tlist,
                                     const double & /* maxdistance */)
{
  try
  {
    TS::TimeSeriesGroupPtr ret(new TS::TimeSeriesGroup);

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

      TS::TimeSeriesPtr timeseries = values(paramOptions, tlist);
      TS::LonLat lonlat(loc->longitude, loc->latitude);

      ret->emplace_back(lonlat, *timeseries);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
TS::TimeSeriesGroupPtr QImpl::valuesAtPressure(const ParameterOptions &param,
                                               const Spine::LocationList &llist,
                                               const TS::TimeSeriesGenerator::LocalTimeList &tlist,
                                               const double & /* maxdistance */,
                                               float pressure)
{
  try
  {
    TS::TimeSeriesGroupPtr ret(new TS::TimeSeriesGroup);

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

      TS::TimeSeriesPtr timeseries = valuesAtPressure(paramOptions, tlist, pressure);
      TS::LonLat lonlat(loc->longitude, loc->latitude);

      ret->emplace_back(lonlat, *timeseries);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
TS::TimeSeriesGroupPtr QImpl::valuesAtHeight(const ParameterOptions &param,
                                             const Spine::LocationList &llist,
                                             const TS::TimeSeriesGenerator::LocalTimeList &tlist,
                                             const double & /* maxdistance */,
                                             float height)
{
  try
  {
    TS::TimeSeriesGroupPtr ret(new TS::TimeSeriesGroup);

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

      TS::TimeSeriesPtr timeseries = valuesAtHeight(paramOptions, tlist, height);
      TS::LonLat lonlat(loc->longitude, loc->latitude);

      ret->emplace_back(lonlat, *timeseries);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Sample the data to create a new Q object
 */
// ----------------------------------------------------------------------

Q QImpl::sample(const Spine::Parameter &theParameter,
                const Fmi::DateTime &theTime,
                const Fmi::SpatialReference &theCrs,
                double theXmin,
                double theYmin,
                double theXmax,
                double theYmax,
                double theResolution)
{
  try
  {
    if (!param(theParameter.number()))
      throw Fmi::Exception(
          BCP,
          "Parameter " + theParameter.name() + " is not available for sampling in the querydata");

    if (theResolution <= 0)
      throw Fmi::Exception(BCP, "The sampling resolution must be nonnegative");

    if (theResolution < 0.01)
      throw Fmi::Exception(BCP, "Sampling resolutions below 10 meters are not supported");

    if (!itsInfo->TimeDescriptor().IsInside(theTime))
      throw Fmi::Exception(BCP, "Cannot sample data to a time outside the querydata");

    if (!itsInfo->IsGrid())
      throw Fmi::Exception(BCP, "Cannot sample point data to new resolution");

    // Establish the new descriptors

    NFmiVPlaceDescriptor vdesc(itsInfo->VPlaceDescriptor());

    NFmiParamBag pbag;
    pbag.Add(itsInfo->Param());
    NFmiParamDescriptor pdesc(pbag);

    NFmiTimeList tlist;
    tlist.Add(new NFmiMetTime(theTime));  // NOLINT(cppcoreguidelines-owning-memory)
    NFmiTimeDescriptor tdesc(itsInfo->OriginTime(), tlist);

    // Establish new projection and the required grid size of the desired resolution

    std::shared_ptr<NFmiArea> newarea(
        NFmiArea::CreateFromBBox(theCrs, NFmiPoint(theXmin, theYmin), NFmiPoint(theXmax, theYmax)));

    double datawidth = newarea->WorldXYWidth() / 1000.0;  // view extent in kilometers
    double dataheight = newarea->WorldXYHeight() / 1000.0;
    int width = static_cast<int>(datawidth / theResolution);
    int height = static_cast<int>(dataheight / theResolution);

    // Must use at least two grid points, value 1 would cause a segmentation fault in here
    width = std::max(width, 2);
    height = std::max(height, 2);

    newarea->SetGridSize(width, height);  // to get fast LatLon access for the grid

    NFmiGrid newgrid(newarea.get(), width, height);
    NFmiHPlaceDescriptor hdesc(newgrid);

    // Then create the new querydata

    NFmiFastQueryInfo newinfo(pdesc, tdesc, hdesc, vdesc);
    std::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(newinfo));
    if (data.get() == nullptr)
      throw Fmi::Exception(BCP, "Failed to create querydata by sampling");

    NFmiFastQueryInfo dstinfo(data.get());
    dstinfo.First();  // sets the only param and time active

    // Now we need all kinds of extra variables because of the damned API

    NFmiPoint dummy;
    std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    Fmi::TimeZonePtr utc("Etc/UTC");
    Fmi::LocalDateTime localdatetime(theTime, utc);

    auto mylocale = std::locale::classic();

    for (dstinfo.ResetLevel(); dstinfo.NextLevel();)
    {
      itsInfo->Level(*dstinfo.Level());
      for (dstinfo.ResetLocation(); dstinfo.NextLocation();)
      {
        auto latlon = dstinfo.LatLon();

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
        if (const auto *ptr = std::get_if<double>(&result))
          dstinfo.FloatValue(*ptr);
      }
    }

    // Return the new Q but with a new hash value

    std::size_t hash = itsHashValue;
    Fmi::hash_combine(hash, Fmi::hash_value(theResolution));
    Fmi::hash_combine(hash, Fmi::hash_value(theTime));
    Fmi::hash_combine(hash, Fmi::hash_value(theXmin));
    Fmi::hash_combine(hash, Fmi::hash_value(theYmin));
    Fmi::hash_combine(hash, Fmi::hash_value(theXmax));
    Fmi::hash_combine(hash, Fmi::hash_value(theYmax));
    Fmi::hash_combine(hash, theCrs.hashValue());

    auto model = Model::create(*itsModels[0], data, hash);
    return std::make_shared<QImpl>(model);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the data hash value
 */
// ----------------------------------------------------------------------

std::size_t QImpl::hashValue() const
{
  return itsHashValue;
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

bool QImpl::needsGlobeWrap() const
{
  return itsInfo->NeedsGlobeWrap();
}

std::size_t hash_value(const Q &theQ)
{
  if (theQ)
    return theQ->hashValue();
  return 666U;
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
