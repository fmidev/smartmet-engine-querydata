
#include "MetaQueryFilters.h"
#include <boost/geometry/geometry.hpp>
#include <spine/Convenience.h>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
using namespace Spine;

namespace bg = boost::geometry;

typedef bg::model::point<double, 2, bg::cs::cartesian> degree_point;

typedef bg::model::box<degree_point> BoxType;

bool filterProducer(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasProducer())
    {
      // No producer specified
      return true;
    }
    else
    {
      return (str_iequal(prop.producer, options.getProducer()));
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterOriginTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasOriginTime())
      // No origin time specified
      return true;

    return (prop.originTime == options.getOriginTime());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterFirstTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasFirstTime())
      // No time specified
      return true;

    return (prop.firstTime == options.getFirstTime());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterLastTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasLastTime())
      // No time specified
      return true;

    return (prop.lastTime == options.getLastTime());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterParameters(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasParameters())
    {
      // No parameters specified
      return true;
    }
    else
    {
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
          // One miss is all we need to fail
          return false;
      }

      // If we made this far, all params have been found
      return true;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterLevelTypes(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasLevelTypes())
    {
      // No parameters specified
      return true;
    }
    else
    {
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
          // One miss is all we need to fail
          return false;
      }

      // If we made this far, all levels have been found
      return true;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterLevelValues(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasLevelValues())
    {
      // No parameters specified
      return true;
    }
    else
    {
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
          // One miss is all we need to fail
          return false;
      }

      // If we made this far, all levels have been found
      return true;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterSynchro(const MetaData& prop, const std::vector<bp::ptime>& originTimes)
{
  try
  {
    for (auto& time : originTimes)
    {
      if (prop.originTime == time)
        // Time is in synchronized origin times
        return true;
    }

    // If we are here, there were no matches
    return false;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool filterBoundingBox(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasBoundingBox())
    {
      // No BB specified
      return true;
    }
    else
    {
      auto givenBox = options.getBoundingBox();
      auto givenurlon = givenBox.ur.X();
      auto givenurlat = givenBox.ur.Y();
      auto givenbllon = givenBox.bl.X();
      auto givenbllat = givenBox.bl.Y();

      //		  BoxType givenGeoBox(degree_point(givenbllon,givenbllat),
      // degree_point(givenurlon,givenurlat));

      degree_point ur(givenurlon, givenurlat);
      degree_point bl(givenbllon, givenbllat);

      BoxType modelGeoBox(degree_point(prop.bllon, prop.bllat),
                          degree_point(prop.urlon, prop.urlat));

      BoxType givenGeoBox(bl, ur);

      // Only accept complete overlap

      // True if given corners are inside the model box
      return (bg::within(givenGeoBox, modelGeoBox));
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
