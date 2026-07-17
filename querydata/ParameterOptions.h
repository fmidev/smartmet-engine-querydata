// ======================================================================
/*!
 * \brief Long parameter list bundled into single struct
 *
 */
// ======================================================================

#pragma once

#include <spine/Location.h>
#include <spine/Parameter.h>
#include <timeseries/TimeSeriesInclude.h>

#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiIndexMask.h>
#include <newbase/NFmiPoint.h>

#include <macgyver/TimeFormatter.h>

#include <macgyver/LocalDateTime.h>

#include <optional>

#include "Producer.h"

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
struct ParameterOptions
{
  ParameterOptions(const Spine::Parameter& theParam,
                   const Producer& theProducer,
                   const Spine::Location& theLocation,
                   const std::string& theCountry,
                   const std::string& thePlace,
                   const Fmi::TimeFormatter& theTimeFormatter,
                   const std::string& theTimeString,
                   const std::string& theLang,
                   const std::locale& theLocale,
                   const std::string& theZone,
                   const bool& theNearestPointFlag,
                   double theMaxDist,
                   NFmiPoint& theLastPoint);

  const Spine::Parameter& par;
  const Producer& producer;
  const Spine::Location& loc;
  const std::string& country;
  const std::string& place;
  const Fmi::TimeFormatter& timeformatter;
  const std::string& timestring;
  const std::string& language;
  const std::locale& outlocale;
  const std::string& outzone;
  const bool& findnearestvalidpoint;
  double maxdist;
  NFmiPoint& lastpoint;

  // Optional reference point for the 'distance' and 'direction' special parameters. When the query
  // point is expanded into several nearby stations (numberofstations>1 for pointwise data), the
  // location coordinates used for data extraction are the station's own coordinates, so distance
  // and direction must instead be measured from the original query point. If unset, the location's
  // own coordinates are used (the normal single-point behaviour).
  std::optional<NFmiPoint> distanceReferencePoint;
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
