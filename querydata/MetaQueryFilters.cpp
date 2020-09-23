
#include "MetaQueryFilters.h"
#include <boost/geometry/geometry.hpp>
#include <spine/Convenience.h>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
namespace bg = boost::geometry;

using Spine::str_iequal;

using degree_point = bg::model::point<double, 2, bg::cs::cartesian>;
using BoxType = bg::model::box<degree_point>;

bool filterProducer(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasProducer())
      return true;  // No producer specified
    return (str_iequal(prop.producer, options.getProducer()));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterOriginTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasOriginTime())
      return true;  // No origin time specified

    return (prop.originTime == options.getOriginTime());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterFirstTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasFirstTime())
      return true;  // No time specified

    return (prop.firstTime == options.getFirstTime());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterLastTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasLastTime())
      return true;  // No time specified

    return (prop.lastTime == options.getLastTime());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterParameters(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasParameters())
      return true;  // No parameters specified

    auto params = options.getParameters();
    for (auto& param : params)
    {
      bool found = false;
      for (auto& fparam : prop.parameters)
      {
        if (str_iequal(fparam.name, param))
        {
          found = true;
          break;
        }
      }
      if (!found)
        return false;  // One miss is all we need to fail
    }

    // If we made this far, all params have been found
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterLevelTypes(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasLevelTypes())
      return true;  // No parameters specified

    auto types = options.getLevelTypes();
    for (auto& type : types)
    {
      bool found = false;
      for (auto& flevel : prop.levels)
      {
        if (str_iequal(flevel.type, type))
        {
          found = true;
          break;
        }
      }

      if (!found)
        return false;  // One miss is all we need to fail
    }

    // If we made this far, all levels have been found
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterLevelValues(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasLevelValues())
      return true;  // No parameters specified

    auto values = options.getLevelValues();
    for (auto& value : values)
    {
      bool found = false;
      for (auto& flevel : prop.levels)
      {
        if (flevel.value == value)
        {
          found = true;
          break;
        }
      }

      if (!found)
        return false;  // One miss is all we need to fail
    }

    // If we made this far, all levels have been found
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterSynchro(const MetaData& prop, const std::vector<bp::ptime>& originTimes)
{
  try
  {
    for (auto& time : originTimes)
    {
      if (prop.originTime == time)
        return true;  // Time is in synchronized origin times
    }

    // If we are here, there were no matches
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterBoundingBox(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasBoundingBox())
      return true;  // No BB specified

    auto givenBox = options.getBoundingBox();
    auto givenurlon = givenBox.ur.X();
    auto givenurlat = givenBox.ur.Y();
    auto givenbllon = givenBox.bl.X();
    auto givenbllat = givenBox.bl.Y();

    //		  BoxType givenGeoBox(degree_point(givenbllon,givenbllat),
    // degree_point(givenurlon,givenurlat));

    degree_point ur(givenurlon, givenurlat);
    degree_point bl(givenbllon, givenbllat);

    BoxType modelGeoBox(degree_point(prop.bllon, prop.bllat), degree_point(prop.urlon, prop.urlat));

    BoxType givenGeoBox(bl, ur);

    // Only accept complete overlap

    // True if given corners are inside the model box
    return (bg::within(givenGeoBox, modelGeoBox));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
