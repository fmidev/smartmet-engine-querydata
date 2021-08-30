#pragma once
#include <boost/shared_ptr.hpp>
#include <newbase/NFmiFastQueryInfo.h>
#include <macgyver/Cache.h>

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
std::shared_ptr<WGS84Envelope> Get(const boost::shared_ptr<NFmiFastQueryInfo>& theInfo);

void SetCacheSize(std::size_t newMaxSize);

const Fmi::Cache::CacheStats& getCacheStats();

}  // namespace WGS84EnvelopeFactory
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
