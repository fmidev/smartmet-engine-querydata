#include "WGS84EnvelopeFactory.h"
#include "Envelope.h"
#include <macgyver/Cache.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
namespace
{
// 512 models should be enough
const int default_cache_size = 512;

using WGS84EnvelopeCache = Fmi::Cache::Cache<std::size_t, std::shared_ptr<WGS84Envelope>>;
WGS84EnvelopeCache g_WGS84GlobalEnvelopeCache{default_cache_size};

}  // namespace

namespace WGS84EnvelopeFactory
{
// Return cached matrix or empty shared_ptr
std::shared_ptr<WGS84Envelope> Get(const boost::shared_ptr<NFmiFastQueryInfo>& theInfo)
{
  std::size_t grid_hash = theInfo->GridHashValue();
  const auto& envelope = g_WGS84GlobalEnvelopeCache.find(grid_hash);

  // If envelope found from cache return it
  if (envelope)
    return *envelope;

  // Create new envelope and add it to cache
  const auto& new_envelope = std::make_shared<WGS84Envelope>(theInfo);
  g_WGS84GlobalEnvelopeCache.insert(grid_hash, new_envelope);
  return new_envelope;
}

// Resize the cache from the default
void SetCacheSize(std::size_t newMaxSize)
{
  g_WGS84GlobalEnvelopeCache.resize(newMaxSize);
}

}  // namespace WGS84EnvelopeFactory
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
