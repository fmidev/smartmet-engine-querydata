// ======================================================================
/*!
 * \brief Information on grid points with valid values
 *
 * We assume the grid may contain points with only missing values
 * due to geographical limitations (sea or land). ValidPoints
 * is constructed for a specific querydata and contains a mask
 * with a boolean for each grid point containing the information
 * for subsequent use.
 *
 * The first use case: a weather forecast for central Helsinki but with
 * waves taken from the nearest valid point in the EC wave model.
 */
// ======================================================================

#pragma once

#include <string>
#include <vector>

class NFmiFastQueryInfo;

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class ValidPoints
{
 public:
  ~ValidPoints();
  ValidPoints(NFmiFastQueryInfo& qinfo, const std::string& cachedir, std::size_t hash);
  bool isvalid(unsigned long index) const;

 private:
  ValidPoints();

  std::vector<bool> itsMask;
  std::string itsCacheFile = "";
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
