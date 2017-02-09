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
 *         number_to_keep          = 1;
 * };
 * \endcode
 */
// ----------------------------------------------------------------------

struct ProducerConfig
{
  ProducerConfig();

  // from the configfile:

  Producer producer;
  std::set<std::string> aliases;
  boost::filesystem::path directory;
  boost::regex pattern;
  std::string type;
  std::string leveltype;
  unsigned int refresh_interval_secs;
  unsigned int number_to_keep;
  double maxdistance;
  bool ismultifile;
  bool isforecast;
  bool isclimatology;
  bool isfullgrid;
  // Monitor index:
};

// Extract producer info from configuration file

ProducerConfig parse_producerinfo(const Producer& producer, const libconfig::Setting& setting);

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
