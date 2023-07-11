
#include "ParameterOptions.h"

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
ParameterOptions::ParameterOptions(const Spine::Parameter& theParam,
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
                                   const TS::LocalTimePoolPtr& theLocalTimePool)
    : par(theParam),
      producer(theProducer),
      loc(theLocation),
      country(theCountry),
      place(thePlace),
      timeformatter(theTimeFormatter),
      timestring(theTimeString),
      language(theLang),
      outlocale(theLocale),
      outzone(theZone),
      findnearestvalidpoint(theNearestPointFlag),
      nearestpoint(theNearestPoint),
      lastpoint(theLastPoint),
      localTimePool(theLocalTimePool)
{
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
