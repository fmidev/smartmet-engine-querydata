#pragma once
#include <boost/shared_ptr.hpp>
#include <newbase/NFmiFastQueryInfo.h>

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
std::shared_ptr<WGS84Envelope> Get(boost::shared_ptr<NFmiFastQueryInfo> theInfo);

void SetCacheSize(std::size_t newMaxSize);

}  // namespace WGS84EnvelopeFactory
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
