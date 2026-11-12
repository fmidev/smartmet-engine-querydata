#pragma once
#include <macgyver/CacheStats.h>
#include <cstddef>
#include <memory>

class NFmiFastQueryInfo;

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// class NFmiFastQueryInfo;
class WGS84Envelope;

namespace WGS84EnvelopeFactory
{
std::shared_ptr<WGS84Envelope> Get(const std::shared_ptr<NFmiFastQueryInfo>& theInfo);

void SetCacheSize(std::size_t newMaxSize);

Fmi::Cache::CacheStats getCacheStats();

}  // namespace WGS84EnvelopeFactory
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
