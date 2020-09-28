// ======================================================================

#include "Producer.h"
#include <macgyver/TimeParser.h>
#include <macgyver/Exception.h>
#include <stdexcept>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// ----------------------------------------------------------------------
/*!
 * \brief Extract producer settings from configuration file
 */
// ----------------------------------------------------------------------

ProducerConfig parse_producerinfo(const Producer &producer, const libconfig::Setting &setting)
{
  std::string name;
  try
  {
    if (!setting.isGroup())
      throw Fmi::Exception(BCP, "Producer settings must be stored in groups delimited by {}!");

    ProducerConfig pinfo;
    pinfo.producer = producer;

    // Implicit casts work for most types, but strings must be cast explicitly
    // to avoid ambiquity between const char * and std::string.

    for (int i = 0; i < setting.getLength(); ++i)
    {
      name = setting[i].getName();

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
	{
	  pinfo.pattern = setting[i];
	  pinfo.pattern_str = setting[i].c_str();
	}

      else if (name == "multifile")
        pinfo.ismultifile = setting[i];

      else if (name == "forecast")
        pinfo.isforecast = setting[i];

      else if (name == "climatology")
        pinfo.isclimatology = setting[i];

      else if (name == "fullgrid")
        pinfo.isfullgrid = setting[i];

      else if (name == "relative_uv")
        pinfo.isrelativeuv = setting[i];

      else if (name == "refresh_interval_secs")
        pinfo.refresh_interval_secs = setting[i];

      else if (name == "number_to_keep")
        pinfo.number_to_keep = setting[i];

      else if (name == "max_age")
        pinfo.max_age = Fmi::TimeParser::parse_duration(setting[i]).total_seconds();

      else if (name == "update_interval")
        pinfo.update_interval = Fmi::TimeParser::parse_duration(setting[i]).total_seconds();

      else if (name == "minimum_expires")
        pinfo.minimum_expires = Fmi::TimeParser::parse_duration(setting[i]).total_seconds();

      else if (name == "maxdistance")
        pinfo.maxdistance = setting[i];

      else if (name == "mmap")
        pinfo.mmap = setting[i];

      else if (name == "type")
        pinfo.type = static_cast<const char *>(setting[i]);

      else if (name == "leveltype")
        pinfo.leveltype = static_cast<const char *>(setting[i]);

      else
        throw Fmi::Exception(BCP,
                               std::string("QEngine: Unknown producer setting named ")
                                   .append(name)
                                   .append(" for producer ")
                                   .append(producer));
    }

    // Sanity checks

    if (pinfo.directory.empty())
      throw Fmi::Exception(BCP, "No directory specified for producer " + producer);

    if (pinfo.pattern_str.empty())
      throw Fmi::Exception(BCP, "No pattern specified for producer " + producer);

    if (pinfo.refresh_interval_secs < 1)
      throw Fmi::Exception(BCP, "Refresh interval for producer " + producer + " must be > 0");

    if (pinfo.number_to_keep < 1)
      throw Fmi::Exception(BCP,
                             "Number of files to keep for producer " + producer + " must be > 0");

    if (pinfo.number_to_keep >= 1000000)
      throw Fmi::Exception(
          BCP, "Number of files to keep for producer " + producer + " must be < 1,000,000");

    if (pinfo.maxdistance >= 10000)
      throw Fmi::Exception(
          BCP, "Maximum search radius for producer " + producer + " must be < 10000 km");

    if (pinfo.update_interval < 60)
      throw Fmi::Exception(BCP,
                             "Minimum update interval for producer " + producer + " is 60 seconds");

    return pinfo;
  }
  catch (...)
  {
    std::string nm;
    if (name.length() > 0)
      nm = " element " + name;
    throw Fmi::Exception::Trace(BCP, "Operation failed for producer " + producer + nm);
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
