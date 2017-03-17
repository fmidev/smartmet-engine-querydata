// ======================================================================
/*!
 * \brief Information on valid points in querydata
 */
// ======================================================================

#include "ValidPoints.h"

#include <spine/Exception.h>
#include <newbase/NFmiFastQueryInfo.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// ----------------------------------------------------------------------
/*!
 * \brief Construct the class from queryinfo
 *
 * Note that we assume the valid grid points do not change rapidly
 * so that we can check only the first and last grid points. For
 * example, we assume the polar ice regions do not change so fast so
 * as to have a significant effect on the valid wave model points.
 *
 */
// ----------------------------------------------------------------------

ValidPoints::ValidPoints(NFmiFastQueryInfo& qinfo) : itsMask(qinfo.SizeLocations(), false)
{
  try
  {
    qinfo.First();
    for (qinfo.ResetLocation(); qinfo.NextLocation();)
    {
      // point is known to be ok for first found valid value

      for (qinfo.ResetLevel(); qinfo.NextLevel();)
      {
        for (qinfo.ResetParam(); qinfo.NextParam();)
        {
#if 1
          // Check only the first and last times
          qinfo.FirstTime();
          if (qinfo.FloatValue() != kFloatMissing)
          {
            itsMask[qinfo.LocationIndex()] = true;
            goto nextpoint;
          }
          qinfo.LastTime();
          if (qinfo.FloatValue() != kFloatMissing)
          {
            itsMask[qinfo.LocationIndex()] = true;
            goto nextpoint;
          }
#else
          // Check all times
          for (qinfo.ResetTime(); qinfo.NextTime();)
          {
            if (qinfo.FloatValue() != kFloatMissing)
            {
              itsMask[qinfo.LocationIndex()] = true;
              goto nextpoint;
            }
          }
#endif
        }
      }
    nextpoint:;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true for valid points
 */
// ----------------------------------------------------------------------

bool ValidPoints::isvalid(unsigned long index) const
{
  try
  {
    if (index >= itsMask.size())
      return false;
    else
      return itsMask[index];
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
