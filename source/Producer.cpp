// ======================================================================

#include "Producer.h"
#include <spine/Exception.h>
#include <stdexcept>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
const int default_refresh_interval_secs = 60;
const int default_number_to_keep = 1;
const double default_maxdistance = -1;  // disabled

// ----------------------------------------------------------------------
/*!
 * \brief Default constructor for ProducerConfig
 */
// ----------------------------------------------------------------------

ProducerConfig::ProducerConfig()
    : producer(),
      aliases(),
      directory(),
      pattern(),
      type("grid"),
      leveltype("surface"),
      refresh_interval_secs(default_refresh_interval_secs),
      number_to_keep(default_number_to_keep),
      maxdistance(default_maxdistance),
      ismultifile(false),
      isforecast(true),
      isclimatology(false),
      isfullgrid(true)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract producer settings from configuration file
 */
// ----------------------------------------------------------------------

ProducerConfig parse_producerinfo(const Producer &producer, const libconfig::Setting &setting)
{
  try
  {
    if (!setting.isGroup())
      throw SmartMet::Spine::Exception(
          BCP, "Producer settings must be stored in groups delimited by {}!");

    ProducerConfig pinfo;
    pinfo.producer = producer;

    // Implicit casts work for most types, but strings must be cast explicitly
    // to avoid ambiquity between const char * and std::string.

    for (int i = 0; i < setting.getLength(); ++i)
    {
      std::string name = setting[i].getName();

      if (name == "alias")
      {
        if (setting[i].isArray())
        {
          for (int j = 0; j < setting[i].getLength(); ++j)
            pinfo.aliases.insert(static_cast<const char *>(setting[i][j]));
        }
        else
          pinfo.aliases.insert(static_cast<const char *>(setting[i]));
      }

      else if (name == "directory")
        pinfo.directory = boost::filesystem::path(static_cast<const char *>(setting[i]));

      else if (name == "pattern")
        pinfo.pattern = setting[i];

      else if (name == "multifile")
        pinfo.ismultifile = setting[i];

      else if (name == "forecast")
        pinfo.isforecast = setting[i];

      else if (name == "climatology")
        pinfo.isclimatology = setting[i];

      else if (name == "fullgrid")
        pinfo.isfullgrid = setting[i];

      else if (name == "refresh_interval_secs")
        pinfo.refresh_interval_secs = setting[i];

      else if (name == "number_to_keep")
        pinfo.number_to_keep = setting[i];

      else if (name == "maxdistance")
        pinfo.maxdistance = setting[i];

      else if (name == "type")
        pinfo.type = static_cast<const char *>(setting[i]);

      else if (name == "leveltype")
        pinfo.leveltype = static_cast<const char *>(setting[i]);

      else
        throw SmartMet::Spine::Exception(
            BCP, "QEngine: Unknown producer setting named " + name + " for producer " + producer);
    }

    // Sanity checks

    if (pinfo.directory.empty())
      throw SmartMet::Spine::Exception(BCP, "No directory specified for producer " + producer);

    if (pinfo.pattern.empty())
      throw SmartMet::Spine::Exception(BCP, "No pattern specified for producer " + producer);

    if (pinfo.refresh_interval_secs < 1)
      throw SmartMet::Spine::Exception(
          BCP, "Refresh interval for producer " + producer + " must be > 0");

    if (pinfo.number_to_keep < 1)
      throw SmartMet::Spine::Exception(
          BCP, "Number of files to keep for producer " + producer + " must be > 0");

    if (pinfo.number_to_keep >= 1000)
      throw SmartMet::Spine::Exception(
          BCP, "Number of files to keep for producer " + producer + " must be < 1000");

    if (pinfo.maxdistance >= 10000)
      throw SmartMet::Spine::Exception(
          BCP, "Maximum search radius for producer " + producer + " must be < 10000 km");

    return pinfo;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
