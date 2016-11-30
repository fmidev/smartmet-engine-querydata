#pragma once

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/shared_ptr.hpp>
#include <list>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
typedef std::list<boost::posix_time::ptime> ValidTimeList;
}  // namspace Q
}  // namspace Engine
}  // namspace SmartMet
