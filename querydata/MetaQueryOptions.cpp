
#include "MetaQueryOptions.h"
#include <spine/Exception.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
MetaQueryOptions::MetaQueryOptions()
    : itsHasProducer(false),
      itsHasOriginTime(false),
      itsHasFirstTime(false),
      itsHasLastTime(false),
      itsHasParameters(false),
      itsHasBoundingBox(false),
      itsHasLevelTypes(false),
      itsHasLevelValues(false)
{
}

void MetaQueryOptions::setProducer(const std::string& producer)
{
  try
  {
    itsProducer = producer;
    itsHasProducer = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool MetaQueryOptions::hasProducer() const
{
  return itsHasProducer;
}

std::string MetaQueryOptions::getProducer() const
{
  return itsProducer;
}

void MetaQueryOptions::setOriginTime(const bp::ptime& originTime)
{
  try
  {
    itsOriginTime = originTime;
    itsHasOriginTime = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool MetaQueryOptions::hasOriginTime() const
{
  return itsHasOriginTime;
}

bp::ptime MetaQueryOptions::getOriginTime() const
{
  return itsOriginTime;
}

void MetaQueryOptions::setFirstTime(const bp::ptime& firstTime)
{
  try
  {
    itsFirstTime = firstTime;
    itsHasFirstTime = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool MetaQueryOptions::hasFirstTime() const
{
  return itsHasFirstTime;
}

bp::ptime MetaQueryOptions::getFirstTime() const
{
  return itsFirstTime;
}

void MetaQueryOptions::setLastTime(const bp::ptime& lastTime)
{
  try
  {
    itsLastTime = lastTime;
    itsHasLastTime = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool MetaQueryOptions::hasLastTime() const
{
  return itsHasLastTime;
}

bp::ptime MetaQueryOptions::getLastTime() const
{
  return itsLastTime;
}

void MetaQueryOptions::addParameter(const std::string& parameter)
{
  try
  {
    itsHasParameters = true;
    itsParameters.push_back(parameter);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool MetaQueryOptions::hasParameters() const
{
  return itsHasParameters;
}

std::list<std::string> MetaQueryOptions::getParameters() const
{
  return itsParameters;
}

void MetaQueryOptions::setBoundingBox(const NFmiPoint& ul,
                                      const NFmiPoint& ur,
                                      const NFmiPoint& bl,
                                      const NFmiPoint& br)
{
  try
  {
    itsBoundingBox = BBox(bl, br, ul, ur);
    itsHasBoundingBox = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void MetaQueryOptions::setBoundingBox(const NFmiPoint& bl, const NFmiPoint& ur)
{
  try
  {
    itsBoundingBox = BBox(bl, ur);
    itsHasBoundingBox = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool MetaQueryOptions::hasBoundingBox() const
{
  return itsHasBoundingBox;
}

MetaQueryOptions::BBox MetaQueryOptions::getBoundingBox() const
{
  return itsBoundingBox;
}

void MetaQueryOptions::addLevelType(const std::string& type)
{
  try
  {
    itsLevelTypes.push_back(type);
    itsHasLevelTypes = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool MetaQueryOptions::hasLevelTypes() const
{
  return itsHasLevelTypes;
}

std::list<std::string> MetaQueryOptions::getLevelTypes() const
{
  return itsLevelTypes;
}

void MetaQueryOptions::addLevelValue(float value)
{
  try
  {
    itsLevelValues.push_back(value);
    itsHasLevelValues = true;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool MetaQueryOptions::hasLevelValues() const
{
  return itsHasLevelValues;
}

std::list<float> MetaQueryOptions::getLevelValues() const
{
  return itsLevelValues;
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet