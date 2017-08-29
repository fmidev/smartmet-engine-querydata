// ======================================================================
/*!
 * \brief Manage thread safe access to the repo
 *
 * The implementation revolves around a couple ideas:
 *
 * # the constructor starts a thread calling DirectoryMonitor::run()
 * # the callback function starts a new thread to load the querydata
 * # once the data is loaded, the internal catalog is updated and the
 *   loading thread exits
 *
 * The constructor is the best place to start the monitoring thread since
 * there we can manage the thread instance and interrupt it if necessary.
 *
 * All users are expected not to modify the querydata.
 *
 * The most common use case is this:
 * \code
 * QEngine::Engine qengine(configfile);
 * QEngine::Model model = qengine.get(producer);
 * NFmiFastQueryInfo qi(sqd.querydata().get());
 * \endcode
 * That is, the qd iterators must not be used directly - instead a new
 * NFmiFastQueryInfo instance must be created, and all data access must
 * be done through it. Any other use case may result in undefined behaviour.
 *
 */
// ======================================================================

#include "RepoManager.h"
#include "Model.h"
#include "Producer.h"
#include "Repository.h"

#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>

#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <cassert>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

RepoManager::~RepoManager()
{
  itsMonitorThread.interrupt();
}
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 *
 * The constructor
 * # parses the configuration file
 * # parses the settings for each producer
 */
// ----------------------------------------------------------------------

RepoManager::RepoManager(const std::string& configfile)
    : itsVerbose(false),
      itsMonitor(),
      itsThreadCount(0),
      itsReady(false),
      itsMaxThreadCount(10),  // default if not configured
      itsShutdownRequested(false),
      itsLatLonCache(500)  // TODO: hard coded 500 different grids

{
  boost::system::error_code ec;

  try
  {
    // This lock is unnecessary since it is not possible to access
    // the object before it has been fully constructed.

    // Spine::WriteLock lock(mutex);

    // Phase 0: Parse configuration file

    try
    {
      // Save the modification time of config to track config changes by other modules
      // Ignoring errors for now, should be caught when reading the file
      std::time_t modtime = boost::filesystem::last_write_time(configfile, ec);
      // There is a slight race condition here: time is recorded before the actual config is read
      // If config changes between these two calls, we actually have old timestamp
      // To minimize the effects, modification time is recorded before reading. May cause almost
      // immediate reread if config file is changing rapidly
      itsConfig.readFile(configfile.c_str());

      itsConfig.lookupValue("maxthreads", itsMaxThreadCount);

      // Options

      itsConfig.lookupValue("verbose", itsVerbose);

      // Phase 1: Establish producer setting

      if (!itsConfig.exists("producers"))
        throw Spine::Exception(BCP, "Configuration file must specify the producers");

      const libconfig::Setting& prods = itsConfig.lookup("producers");

      if (!prods.isArray())
        throw Spine::Exception(BCP, "Configured value of 'producers' must be an array");

      // Phase 2: Parse individual producer settings

      for (int i = 0; i < prods.getLength(); ++i)
      {
        Producer prod = prods[i];

        if (!itsConfig.exists(prod))
          throw Spine::Exception(BCP, "Producer settings for " + prod + " are missing");

        ProducerConfig pinfo = parse_producerinfo(prod, itsConfig.lookup(prod));

        // Save the info

        itsConfigList.push_back(pinfo);
      }

      this->configModTime = modtime;
    }
    catch (libconfig::ParseException& e)
    {
      throw Spine::Exception(BCP,
                             "Qengine configuration " + configfile + " error '" +
                                 std::string(e.getError()) + "' on line " +
                                 std::to_string(e.getLine()));
    }
    catch (libconfig::ConfigException& e)
    {
      throw Spine::Exception(BCP, configfile + ": " + std::strerror(ec.value()));
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the manager
 *
 * The constructor merely parses the configuration file, the actual
 * work is done here.
 */
// ----------------------------------------------------------------------

void RepoManager::init()
{
  try
  {
    BOOST_FOREACH (const auto& pinfo, itsConfigList)
    {
      // Note: watcher indexes start from 0, so we can index the producer
      // with a vector to find out which producer the callback instructs to update.
      // Should be fast.

      auto id = itsMonitor.watch(pinfo.directory,
                                 pinfo.pattern,
                                 boost::bind(&RepoManager::update, this, _1, _2, _3, _4),
                                 boost::bind(&RepoManager::error, this, _1, _2, _3, _4),
                                 pinfo.refresh_interval_secs,
                                 Fmi::DirectoryMonitor::CREATE | Fmi::DirectoryMonitor::DELETE);

      // Save the info

      itsRepo.add(pinfo);
      itsProducerList.push_back(pinfo.producer);
      itsProducerMap.insert(ProducerMap::value_type(id, pinfo.producer));
    }

    itsMonitorThread = boost::thread(boost::bind(&Fmi::DirectoryMonitor::run, &itsMonitor));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown
 */
// ----------------------------------------------------------------------

void RepoManager::shutdown()
{
  try
  {
    itsShutdownRequested = true;

    std::cout << "  -- Shutdown requested (RepoManager)\n";
    itsMonitor.stop();

    while (itsThreadCount > 0)
    {
      std::cout << "    - threads : " << itsThreadCount << "\n";
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void RepoManager::shutdownRequestFlagSet()
{
  itsShutdownRequested = true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get id for given producer
 */
// ----------------------------------------------------------------------

Fmi::DirectoryMonitor::Watcher RepoManager::id(const Producer& producer) const
{
  try
  {
    // no lock needed, this method is private, caller is responsible

    BOOST_FOREACH (const ProducerMap::value_type& it, itsProducerMap)
    {
      if (it.second == producer)
        return it.first;
    }

    throw Spine::Exception(BCP, "Request for unknown producer!");
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Error callback function
 *
 * Should we unload all files just in case? Really depends
 * on the behaviour or DirectoryMonitor. Perhaps it should
 * reset its state, and everything else would be automatic?
 */
// ----------------------------------------------------------------------

void RepoManager::error(Fmi::DirectoryMonitor::Watcher /* id */,
                        boost::filesystem::path dir,
                        boost::regex /* pattern */,
                        std::string message)
{
  try
  {
    std::cout << ANSI_FG_RED << "Error in directory " << dir << " : " << message << ANSI_FG_DEFAULT
              << std::endl;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Update callback function
 *
 * Things to do:
 *
 * # see if any loaded files have been deleted
 * # see if any new files have been created
 *
 * We ignore modified files in the monitor status call. However,
 * if any callback request notices a modified file, we will
 * reload it. Users should not trust that the mechanism is safe,
 * since any access to deleted data is likely to cause a bus error.
 * It would be pure luck to notice a deleted file before someone
 * uses it.
 */
// ----------------------------------------------------------------------

void RepoManager::update(Fmi::DirectoryMonitor::Watcher id,
                         boost::filesystem::path /* dir */,
                         boost::regex /* pattern */,
                         Fmi::DirectoryMonitor::Status status)
{
  try
  {
    const Producer& producer = itsProducerMap.find(id)->second;

    // Collect names of files to be unloaded or loaded

    Files removals;
    Files additions;
    BOOST_FOREACH (const auto& file_status, *status)
    {
      if (file_status.second == Fmi::DirectoryMonitor::DELETE ||
          file_status.second == Fmi::DirectoryMonitor::MODIFY)
      {
        removals.push_back(file_status.first);
      }
      else if (file_status.second == Fmi::DirectoryMonitor::CREATE ||
               file_status.second == Fmi::DirectoryMonitor::MODIFY)
      {
        additions.push_back(file_status.first);
      }
    }

    // Handle deleted files

    if (!removals.empty())
    {
      // Take the lock only when needed
      Spine::WriteLock lock(itsMutex);
      BOOST_FOREACH (const auto& file, removals)
        itsRepo.remove(producer, file);
    }

    // Done if there are no additions

    if (additions.empty())
      return;

    // We limit the number of threads to avoid exhausting the system

    bool ok = false;
    while (!ok && !itsShutdownRequested)
    {
      {
        Spine::ReadLock lock(itsThreadCountMutex);
        if (itsThreadCount <= itsMaxThreadCount)
          ok = true;
      }
      if (!ok)
        boost::this_thread::sleep(boost::posix_time::milliseconds(50));
    }

    // Abort if there is a shut down request
    if (itsShutdownRequested)
      return;

    // Note: We are really counting scheduled threads, not
    // ones which have actually started. Hence the counter
    // should be here and not in the load method.
    {
      Spine::WriteLock lock(itsThreadCountMutex);
      ++itsThreadCount;
    }

// Handle new or modified files

#if 0
	std::cerr << ANSI_FG_GREEN << "Threads: " << itsThreadCount
			  << " " << filename << ANSI_FG_DEFAULT << std::endl;
#endif
    boost::thread thrd(boost::bind(&RepoManager::load, this, producer, additions));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Querydata loader function
 *
 * This should be run as a separate thread. Arguments are
 * copies instead of references intentionally.
 */
// ----------------------------------------------------------------------

void RepoManager::load(Producer producer, Files files)
{
  // Just in case
  if (files.empty())
    return;

  if (itsShutdownRequested)
  {
    Spine::WriteLock lock(itsThreadCountMutex);
    --itsThreadCount;
    return;
  }

  // We expect timestamps and want the newest file first
  std::sort(files.rbegin(), files.rend());

  const ProducerConfig& conf = producerConfig(producer);

  unsigned int successful_loads = 0;

  for (const auto& filename : files)
  {
    // Done if the remaining files would not be accepted for being older
    if (successful_loads >= conf.number_to_keep)
      break;

    // files may be corrupt, hence we catch exceptions
    try
    {
      if (itsVerbose)
      {
        std::ostringstream msg;
        msg << Spine::log_time_str() << " QENGINE LOAD " << filename << std::endl;
        std::cout << msg.str() << std::flush;
      }

      SharedModel model = boost::make_shared<Model>(
          filename, conf.producer, conf.leveltype, conf.isclimatology, conf.isfullgrid);

      if (itsVerbose)
      {
        std::ostringstream msg;
        msg << Spine::log_time_str() << " QENGINE ORIGINTIME for " << filename << " is "
            << model->originTime() << std::endl;
        std::cout << msg.str() << std::flush;
      }

      // Update latlon-cache if necessary. In any case make sure model cache is up to date

      auto hash = model->gridHashValue();
      auto latlons = itsLatLonCache.find(hash);  // cached coordinates, if any
      if (!latlons)
        itsLatLonCache.insert(hash, model->makeLatLonCache());  // calc latloncache and cache it
      else
        model->setLatLonCache(*latlons);  // set model cache from our cache

      {
        // update structures safely

        Spine::WriteLock lock(itsMutex);
        itsRepo.add(producer, model);
        ++successful_loads;
        itsRepo.resize(producer, conf.number_to_keep);
      }
    }
    catch (...)
    {
      Spine::Exception exception(BCP, "QEngine failed to load the file!", NULL);
      exception.addParameter("File", filename.c_str());
      std::cerr << exception.getStackTrace();
    }
  }  // for all files

  Spine::WriteLock lock(itsThreadCountMutex);
  --itsThreadCount;

  // Set ready flag if the scan is complete. Only the 1st full scan changes the state.

  if (itsThreadCount == 0 && itsMonitor.ready())
    itsReady = true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the repositories have been scanned at least once
 */
// ----------------------------------------------------------------------

bool RepoManager::ready() const
{
  return itsReady;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the config for the given producer
 */
// ----------------------------------------------------------------------

const ProducerConfig& RepoManager::producerConfig(const Producer& producer) const
{
  try
  {
    // I think there should be a lock here but using one
    // jams the server. Must study more carefully.
    // Spine::ReadLock lock(mutex);

    BOOST_FOREACH (const ProducerConfig& config, itsConfigList)
    {
      if (config.producer == producer)
        return config;
    }

    // NOT REACHED
    throw Spine::Exception(BCP, "Unknown producer config '" + producer + "' requested");
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
