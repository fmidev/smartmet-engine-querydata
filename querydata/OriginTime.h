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
using OriginTime = boost::posix_time::ptime;
using OriginTimes = std::set<OriginTime>;
using OriginTimePeriod = boost::posix_time::time_period;

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
