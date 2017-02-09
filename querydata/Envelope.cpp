#include "Envelope.h"

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
WGS84Envelope::WGS84Envelope() : mRangeLon(-180.0, 180.0), mRangeLat(-90.0, 90.0)
{
}

WGS84Envelope::WGS84Envelope(const WGS84Envelope& other)
    : mRangeLon(other.getRangeLon()), mRangeLat(other.getRangeLat())
{
}

WGS84Envelope::WGS84Envelope(const boost::shared_ptr<NFmiFastQueryInfo> info)
    : mRangeLon(-180.0, 180.0), mRangeLat(-90.0, 90.0)
{
  // Calculate latlon boundary
  info->ResetParam();
  info->NextParam(false);
  const NFmiPoint& b2Point = info->LatLon(0);
  mRangeLon.set(b2Point.X(), b2Point.X());
  mRangeLat.set(b2Point.Y(), b2Point.Y());
  const unsigned long nX = info->GridXNumber();
  const unsigned long nY = info->GridYNumber();
  for (unsigned long yId = 0; yId < nY; yId++)
  {
    unsigned long baseId = yId * nX;
    for (unsigned long id = baseId; id < baseId + nX; id++)
    {
      const NFmiPoint& b2Point = info->LatLon(id);
      mRangeLon.set(std::min(mRangeLon.getMin(), b2Point.X()),
                    std::max(mRangeLon.getMax(), b2Point.X()));
      mRangeLat.set(std::min(mRangeLat.getMin(), b2Point.Y()),
                    std::max(mRangeLat.getMax(), b2Point.Y()));
      // Only the first and the last index in the lines [1,nY-1]
      if (id == baseId and yId != 0 and yId != nY - 1)
        id += nX - 2;
    }
  }
}

const WGS84Envelope& WGS84Envelope::operator=(const WGS84Envelope& other)
{
  mRangeLon = other.getRangeLon();
  mRangeLat = other.getRangeLat();
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
}
}
}
