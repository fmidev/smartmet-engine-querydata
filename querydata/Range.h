#pragma once

#include <memory>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class Range
{
 public:
  using Shared = std::shared_ptr<Range>;
  using Unique = std::unique_ptr<Range>;
  using ValueType = double;
  Range(const ValueType& first, const ValueType& second);

  ~Range() = default;
  Range() = delete;
  Range(const Range& other) = default;
  Range& operator=(const Range& other) = default;
  Range(Range&& other) = default;
  Range& operator=(Range&& other) = default;

  ValueType getMin() const;
  ValueType getMax() const;
  void set(const ValueType& a, const ValueType& b);

 private:
  ValueType mMin;
  ValueType mMax;
};
}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
