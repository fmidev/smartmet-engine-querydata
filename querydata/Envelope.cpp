#include "Envelope.h"
#include <newbase/NFmiFastQueryInfo.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
WGS84Envelope::WGS84Envelope() : mRangeLon(-180.0, 180.0), mRangeLat(-90.0, 90.0) {}

WGS84Envelope::WGS84Envelope(const WGS84Envelope& other)
    : mRangeLon(other.getRangeLon()), mRangeLat(other.getRangeLat())
{
}

WGS84Envelope::WGS84Envelope(const boost::shared_ptr<NFmiFastQueryInfo>& info)
    : mRangeLon(-180.0, 180.0), mRangeLat(-90.0, 90.0)
{
  // Calculate latlon boundary
  info->FirstParam();

  // TODO: This algorithm is bugged, it will not work for arctic data where the pole is in the
  // middle of the data
  if (info->Area() != nullptr)
  {
    // Gridded data
    const NFmiPoint& point = info->LatLon(0);
    mRangeLon.set(point.X(), point.X());
    mRangeLat.set(point.Y(), point.Y());
    const auto nX = info->GridXNumber();
    const auto nY = info->GridYNumber();
    for (std::size_t yId = 0; yId < nY; yId++)
    {
      auto baseId = yId * nX;
      for (std::size_t id = baseId; id < baseId + nX; id++)
      {
        const NFmiPoint& p = info->LatLon(id);
        mRangeLon.set(std::min(mRangeLon.getMin(), p.X()), std::max(mRangeLon.getMax(), p.X()));
        mRangeLat.set(std::min(mRangeLat.getMin(), p.Y()), std::max(mRangeLat.getMax(), p.Y()));
        // Only the first and the last index in the lines [1,nY-1]
        if (id == baseId and yId != 0 and yId != nY - 1)
          id += nX - 2;
      }
    }
  }
  else
  {
    // Point data
    bool first = true;
    for (info->ResetLocation(); info->NextLocation();)
    {
      NFmiPoint point = info->LatLon();
      if (point.X() != kFloatMissing && point.Y() != kFloatMissing)
      {
        if (first)
        {
          mRangeLon.set(point.X(), point.X());
          mRangeLat.set(point.Y(), point.Y());
          first = false;
        }
        else
        {
          mRangeLon.set(std::min(mRangeLon.getMin(), point.X()),
                        std::max(mRangeLon.getMax(), point.X()));
          mRangeLat.set(std::min(mRangeLat.getMin(), point.Y()),
                        std::max(mRangeLat.getMax(), point.Y()));
        }
      }
    }
  }
}

WGS84Envelope& WGS84Envelope::operator=(const WGS84Envelope& other)
{
  if (this != &other)
  {
    mRangeLon = other.getRangeLon();
    mRangeLat = other.getRangeLat();
  }
  return *this;
}

const WGS84Envelope::RangeLon& WGS84Envelope::getRangeLon() const
{
  return mRangeLon;
}

const WGS84Envelope::RangeLat& WGS84Envelope::getRangeLat() const
{
  return mRangeLat;
}
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
