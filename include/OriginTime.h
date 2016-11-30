// ======================================================================
/*!
 * \brief Origin time definition
 */
// ======================================================================

#pragma once

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <set>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
typedef boost::posix_time::ptime OriginTime;
typedef std::set<OriginTime> OriginTimes;
typedef boost::posix_time::time_period OriginTimePeriod;

}  // namspace Q
}  // namspace Engine
}  // namspace SmartMet

// ======================================================================
