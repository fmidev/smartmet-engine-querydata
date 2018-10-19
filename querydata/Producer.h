#pragma once

#include <libconfig.h++>

#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <list>
#include <set>
#include <string>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// A producer is identified by its name

typedef std::string Producer;

// A list of producers

typedef std::list<std::string> ProducerList;

// ----------------------------------------------------------------------
/*!
 * \brief Information on a single producer
 *
 * Sample config:
 * \code
 * pal_skandinavia:
 * {
 *         alias                   = ["pal","kap"];
 *         directory               = "/smartmet/src/cpp/bin/brainstorm/data/pal";
 *         pattern                 = ".*_pal_skandinavia_pinta\.sqd$"
 *         ismultifile             = false;
 *         forecast                = true;
 *         climatology             = false;
 *         type                    = "grid";
 *         leveltype               = "surface";
 *         refresh_interval_secs   = 60;
 *         max_age                 = "PT24H";
 *         number_to_keep          = 2;
 *         mmap                    = true;
 *         update_interval         = "PT1H";
 *         minimum_expires         = "PT5M";
 *         relative_uv             = false;
 * };
 * \endcode
 */
// ----------------------------------------------------------------------

struct ProducerConfig
{
  ProducerConfig() = default;

  // from the configfile:

  Producer producer;
  std::set<std::string> aliases;
  boost::filesystem::path directory;
  boost::regex pattern;
  std::string type = "grid";
  std::string leveltype = "surface";
  unsigned int refresh_interval_secs = 60;  // once per minute
  unsigned int number_to_keep = 2;          // at least two files!
  unsigned int update_interval = 3600;      // once per hour
  unsigned int minimum_expires = 600;       // 10 minutes
  unsigned int max_age = 0;                 // do not remove old models by default based on age
  double maxdistance = -1;
  bool ismultifile = false;
  bool isforecast = true;
  bool isclimatology = false;
  bool isfullgrid = true;
  bool isrelativeuv = false;  // are U/V winds relative to grid orientation
  bool mmap = true;

  // Note: If number_to_keep is only one, during the one minute refresh interval a qengine
  // status query might see a new file in some backends and an older one in others. There
  // would be no common content, which may mess up production.

  inline bool operator==(const ProducerConfig& c)
  {
    return c.isfullgrid == isfullgrid && c.isclimatology == isclimatology &&
           c.isforecast == isforecast && c.ismultifile == ismultifile &&
           c.maxdistance == maxdistance && c.number_to_keep == number_to_keep &&
           c.update_interval == update_interval && c.minimum_expires == minimum_expires &&
           c.max_age == max_age && c.refresh_interval_secs == refresh_interval_secs &&
           c.leveltype == leveltype && c.type == type && c.pattern == pattern &&
           c.directory == directory && c.aliases == aliases && c.producer == producer &&
           c.max_age == max_age && c.isrelativeuv == isrelativeuv && c.mmap == mmap;
  }
  inline bool operator!=(const ProducerConfig& c) { return !operator==(c); }

  // Monitor index:
};

// Extract producer info from configuration file

ProducerConfig parse_producerinfo(const Producer& producer, const libconfig::Setting& setting);

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
