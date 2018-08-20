// ======================================================================
/*!
 * \brief Information on valid points in querydata
 */
// ======================================================================

#include "ValidPoints.h"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/serialization/vector.hpp>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <fstream>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// ----------------------------------------------------------------------
/*!
 * \brief Clean up the cached points
 */
// ----------------------------------------------------------------------

void ValidPoints::uncache() const
{
  if (itsCacheFile.empty())
    return;

  // We ignore errors on purpose
  boost::system::error_code ec;
  boost::filesystem::remove(itsCacheFile, ec);
}

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

ValidPoints::ValidPoints(NFmiFastQueryInfo& qinfo, const std::string& cachedir, std::size_t hash)
    : itsMask(qinfo.SizeLocations(), false)
{
  itsCacheFile = cachedir + "/" + Fmi::to_string(hash);

  if (!boost::filesystem::is_directory(cachedir))
    boost::filesystem::create_directories(cachedir);

  // Try using a cached file first
  try
  {
    if (boost::filesystem::exists(itsCacheFile))
    {
      std::ifstream file(itsCacheFile);
      boost::archive::binary_iarchive archive(file);
      archive& BOOST_SERIALIZATION_NVP(itsMask);
      return;
    }
  }
  catch (...)
  {
    std::cerr << Spine::log_time_str() << " failed to unserialize " << itsCacheFile << std::endl;
  }

  // Calculate from querydata and cache the results
  try
  {
    // Speed up changing between times
    qinfo.FirstTime();
    auto first_time = qinfo.TimeIndex();
    qinfo.LastTime();
    auto last_time = qinfo.TimeIndex();

    for (qinfo.ResetLocation(); qinfo.NextLocation();)
    {
      // seek any valid value for the point

      for (qinfo.ResetParam(); qinfo.NextParam();)
      {
        for (qinfo.ResetLevel(); qinfo.NextLevel();)
        {
          // Check only the first and last times for speed
          qinfo.TimeIndex(first_time);
          if (qinfo.FloatValue() != kFloatMissing)
          {
            itsMask[qinfo.LocationIndex()] = true;
            goto nextpoint;
          }
          qinfo.TimeIndex(last_time);
          if (qinfo.FloatValue() != kFloatMissing)
          {
            itsMask[qinfo.LocationIndex()] = true;
            goto nextpoint;
          }
        }
      }
    nextpoint:;
    }

    try
    {
      // We use a temporary file just in case a shutdown comes during the serialization
      std::string tmpfile = itsCacheFile + ".tmp";
      std::ofstream file(tmpfile);
      boost::archive::binary_oarchive archive(file);
      archive& BOOST_SERIALIZATION_NVP(itsMask);
      boost::filesystem::rename(tmpfile, itsCacheFile);
    }
    catch (...)
    {
      std::cerr << Spine::log_time_str() << " failed to serialize " << itsCacheFile << std::endl;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}  // namespace Querydata

// ----------------------------------------------------------------------
/*!
 * \brief Return true for valid points
 */
// ----------------------------------------------------------------------

bool ValidPoints::isvalid(std::size_t index) const
{
  try
  {
    if (index >= itsMask.size())
      return false;
    return itsMask[index];
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
