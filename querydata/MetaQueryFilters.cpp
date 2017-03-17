
#include "MetaQueryFilters.h"

#include <boost/foreach.hpp>
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
      if (str_iequal(prop.producer, options.getProducer()))
      {
        return true;
      }
      else
      {
        return false;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool filterOriginTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasOriginTime())
    {
      // No origin time specified
      return true;
    }
    else
    {
      if (prop.originTime == options.getOriginTime())
      {
        return true;
      }
      else
      {
        return false;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool filterFirstTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasFirstTime())
    {
      // No time specified
      return true;
    }
    else
    {
      if (prop.firstTime == options.getFirstTime())
      {
        return true;
      }
      else
      {
        return false;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool filterLastTime(const MetaData& prop, const MetaQueryOptions& options)
{
  try
  {
    if (!options.hasLastTime())
    {
      // No time specified
      return true;
    }
    else
    {
      if (prop.lastTime == options.getLastTime())
      {
        return true;
      }
      else
      {
        return false;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      BOOST_FOREACH (auto& param, params)
      {
        bool found = false;
        BOOST_FOREACH (auto& fparam, prop.parameters)
        {
          if (str_iequal(fparam.name, param))
          {
            found = true;
            break;
          }
        }
        if (found == false)
        {
          // One miss is all we need to fail
          return false;
        }
      }

      // If we made this far, all params have been found
      return true;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      BOOST_FOREACH (auto& type, types)
      {
        bool found = false;
        BOOST_FOREACH (auto& flevel, prop.levels)
        {
          if (str_iequal(flevel.type, type))
          {
            found = true;
            break;
          }
        }
        if (found == false)
        {
          // One miss is all we need to fail
          return false;
        }
      }

      // If we made this far, all levels have been found
      return true;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      BOOST_FOREACH (auto& value, values)
      {
        bool found = false;
        BOOST_FOREACH (auto& flevel, prop.levels)
        {
          if (flevel.value == value)
          {
            found = true;
            break;
          }
        }
        if (found == false)
        {
          // One miss is all we need to fail
          return false;
        }
      }

      // If we made this far, all levels have been found
      return true;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool filterSynchro(const MetaData& prop, const std::vector<bp::ptime>& originTimes)
{
  try
  {
    BOOST_FOREACH (auto& time, originTimes)
    {
      if (prop.originTime == time)
      {
        // Time is in synchronized origin times
        return true;
      }
    }

    // If we are here, there were no matches
    return false;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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

      if (bg::within(givenGeoBox, modelGeoBox))
      {
        // Given corners are inside the model box
        return true;
      }
      else
      {
        return false;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet
