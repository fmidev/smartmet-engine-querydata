#include "Range.h"

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
Range::Range(const ValueType& first, const ValueType& second)
    : mMin(std::min(first, second)), mMax(std::max(first, second))
{
}

Range::ValueType Range::getMin() const
{
  return mMin;
}

Range::ValueType Range::getMax() const
{
  return mMax;
}

void Range::set(const ValueType& a, const ValueType& b)
{
  mMin = std::min(a, b);
  mMax = std::max(a, b);
}
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
