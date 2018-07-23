#pragma once

#include "Range.h"
#include <newbase/NFmiFastQueryInfo.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class WGS84Envelope
{
 public:
  using Shared = std::shared_ptr<WGS84Envelope>;
  using Unique = std::unique_ptr<WGS84Envelope>;
  using RangeLon = Range;
  using RangeLat = Range;

  /* Default range: Latitude(-90,90) Longitude(-180,180) */
  explicit WGS84Envelope();
  explicit WGS84Envelope(const WGS84Envelope& other);
  explicit WGS84Envelope(const boost::shared_ptr<NFmiFastQueryInfo> info);
  WGS84Envelope& operator=(const WGS84Envelope& other);
  const RangeLon& getRangeLon() const;
  const RangeLat& getRangeLat() const;

 private:
  RangeLon mRangeLon;
  RangeLat mRangeLat;
};
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
