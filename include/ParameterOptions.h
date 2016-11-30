// ======================================================================
/*!
 * \brief Long parameter list bundled into single struct
 *
 */
// ======================================================================

#pragma once

#include <spine/Location.h>
#include <spine/Parameter.h>
#include <spine/TimeSeriesGenerator.h>

#include <newbase/NFmiPoint.h>
#include <newbase/NFmiIndexMask.h>
#include <newbase/NFmiFastQueryInfo.h>

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
  ParameterOptions(const SmartMet::Spine::Parameter& theParam,
                   const Producer& theProducer,
                   const SmartMet::Spine::Location& theLocation,
                   const std::string& theCountry,
                   const std::string& thePlace,
                   const Fmi::TimeFormatter& theTimeFormatter,
                   const std::string& theTimeString,
                   const std::string& theLang,
                   const std::locale& theLocale,
                   const std::string& theZone,
                   const bool& theNearestPointFlag,
                   const NFmiPoint& theNearestPoint,
                   NFmiPoint& theLastPoint);

  const SmartMet::Spine::Parameter& par;
  const Producer& producer;
  const SmartMet::Spine::Location& loc;
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
};

}  // namspace Q
}  // namspace Engine
}  // namspace SmartMet
