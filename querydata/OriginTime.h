// ======================================================================
/*!
 * \brief Origin time definition
 */
// ======================================================================

#pragma once

#include <macgyver/DateTime.h>
#include <set>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
using OriginTime = Fmi::DateTime;
using OriginTimes = std::set<OriginTime>;
using OriginTimePeriod = boost::posix_time::time_period;

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
