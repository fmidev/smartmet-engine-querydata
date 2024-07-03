#pragma once

#include "Range.h"
#include <boost/move/unique_ptr.hpp>
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
  using Unique = boost::movelib::unique_ptr<WGS84Envelope>;
  using RangeLon = Range;
  using RangeLat = Range;

  /* Default range: Latitude(-90,90) Longitude(-180,180) */
  ~WGS84Envelope() = default;
  WGS84Envelope();
  explicit WGS84Envelope(const WGS84Envelope& other);
  explicit WGS84Envelope(const std::shared_ptr<NFmiFastQueryInfo>& info);
  WGS84Envelope& operator=(const WGS84Envelope& other);

  WGS84Envelope(WGS84Envelope&& other) = delete;
  WGS84Envelope& operator=(WGS84Envelope&& other) = delete;

  const RangeLon& getRangeLon() const;
  const RangeLat& getRangeLat() const;

 private:
  RangeLon mRangeLon;
  RangeLat mRangeLat;
};
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
