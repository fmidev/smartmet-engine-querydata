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

#include <boost/date_time/local_time/local_date_time.hpp>

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
                   const NFmiPoint& theNearestPoint,
                   NFmiPoint& theLastPoint,
				   TS::LocalTimePoolPtr theLocalTimePool);

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
  const NFmiPoint& nearestpoint;
  NFmiPoint& lastpoint;
  TS::LocalTimePoolPtr localTimePool; 
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
