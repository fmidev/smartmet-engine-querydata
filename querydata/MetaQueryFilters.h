
#pragma once

#include "MetaData.h"
#include "MetaQueryOptions.h"

#include <macgyver/DateTime.h>
#include <vector>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
bool filterProducer(const MetaData& prop, const MetaQueryOptions& options);

bool filterOriginTime(const MetaData& prop, const MetaQueryOptions& options);

bool filterFirstTime(const MetaData& prop, const MetaQueryOptions& options);

bool filterLastTime(const MetaData& prop, const MetaQueryOptions& options);

bool filterParameters(const MetaData& prop, const MetaQueryOptions& options);

bool filterBoundingBox(const MetaData& prop, const MetaQueryOptions& options);

bool filterLevelTypes(const MetaData& prop, const MetaQueryOptions& options);

bool filterLevelValues(const MetaData& prop, const MetaQueryOptions& options);

bool filterSynchro(const MetaData& prop, const std::vector<Fmi::DateTime>& originTimes);

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
