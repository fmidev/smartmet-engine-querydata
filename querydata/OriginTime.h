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
using OriginTimePeriod = Fmi::TimePeriod;

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
