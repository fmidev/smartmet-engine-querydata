#pragma once

#include "Envelope.h"
#include "Producer.h"
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
  ModelParameter(std::string theName, std::string theDesc, int thePrecision);
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
  ModelLevel(std::string theType, std::string theName, float theValue);
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
  long timeStep = 0;    // Minutes
  long nTimeSteps = 0;  // number of timesteps

  std::string WKT;

  double ullon = 0;  // Area corners
  double ullat = 0;
  double urlon = 0;
  double urlat = 0;
  double bllon = 0;
  double bllat = 0;
  double brlon = 0;
  double brlat = 0;
  double clon = 0;
  double clat = 0;

  unsigned int xNumber = 0;
  unsigned int yNumber = 0;
  double xResolution = 0;  // in km
  double yResolution = 0;

  double areaWidth = 0;  // in km
  double areaHeight = 0;

  double aspectRatio = 0;

  std::list<ModelLevel> levels;
  std::list<ModelParameter> parameters;
  WGS84Envelope wgs84Envelope;
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
