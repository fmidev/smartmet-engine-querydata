
#pragma once

#include "MetaData.h"
#include "MetaQueryOptions.h"

#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
namespace bp = boost::posix_time;

bool filterProducer(const MetaData& prop, const MetaQueryOptions& options);

bool filterOriginTime(const MetaData& prop, const MetaQueryOptions& options);

bool filterFirstTime(const MetaData& prop, const MetaQueryOptions& options);

bool filterLastTime(const MetaData& prop, const MetaQueryOptions& options);

bool filterParameters(const MetaData& prop, const MetaQueryOptions& options);

bool filterBoundingBox(const MetaData& prop, const MetaQueryOptions& options);

bool filterLevelTypes(const MetaData& prop, const MetaQueryOptions& options);

bool filterLevelValues(const MetaData& prop, const MetaQueryOptions& options);

bool filterSynchro(const MetaData& prop, const std::vector<bp::ptime>& originTimes);

}  // namspace Q
}  // namspace Engine
}  // namspace SmartMet
