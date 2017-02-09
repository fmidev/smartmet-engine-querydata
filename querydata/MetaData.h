#pragma once

#include "Producer.h"
#include "Envelope.h"
#include <boost/date_time/posix_time/posix_time.hpp>

#include <list>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// ----------------------------------------------------------------------
/*!
 * Model parameter metadata container
 *
 */
// ----------------------------------------------------------------------

struct ModelParameter
{
  ModelParameter(const std::string& theName, const std::string& theDesc, int thePrecision);
  std::string name;
  std::string description;
  int precision;
};

// ----------------------------------------------------------------------
/*!
 * Model level metadata container
 *
 */
// ----------------------------------------------------------------------

struct ModelLevel
{
  ModelLevel(const std::string& theType, const std::string& theName, float theValue);
  std::string type;
  std::string name;
  float value;
};

// ----------------------------------------------------------------------
/*!
 * Forecast model metadata container
 *
 */
// ----------------------------------------------------------------------

struct MetaData
{
  MetaData();
  Producer producer;
  boost::posix_time::ptime originTime;
  boost::posix_time::ptime firstTime;
  boost::posix_time::ptime lastTime;
  long timeStep;    // Minutes
  long nTimeSteps;  // number of timesteps

  std::string WKT;

  double ullon;  // Area corners
  double ullat;
  double urlon;
  double urlat;
  double bllon;
  double bllat;
  double brlon;
  double brlat;
  double clon;
  double clat;

  unsigned int xNumber;
  unsigned int yNumber;
  double xResolution;  // in km
  double yResolution;

  double areaWidth;  // in km
  double areaHeight;

  double aspectRatio;

  std::list<ModelLevel> levels;
  std::list<ModelParameter> parameters;
  WGS84Envelope wgs84Envelope;
};

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet
