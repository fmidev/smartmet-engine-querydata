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
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <spine/Convenience.h>
#include <macgyver/Exception.h>
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
namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Return a setting, which may have a host specific value
 *
 * Example:
 *
 *   verbose = false;
 *
 *   overrides:
 *   (
 *       {
 *           name = ["host1","host2"];
 *           verbose = true;
 *       };
 *       ...
 *   )
 */
// ----------------------------------------------------------------------

template <typename T>
bool lookupHostSetting(const libconfig::Config& theConfig,
                       T& theValue,
                       const std::string& theVariable,
                       const std::string& theHost)
{
  try
  {
    // scan for overrides
    if (theConfig.exists("overrides"))
    {
      const libconfig::Setting& override = theConfig.lookup("overrides");
      int count = override.getLength();
      for (int i = 0; i < count; ++i)
      {
        const libconfig::Setting& trial_hosts = override[i]["name"];
        int numhosts = trial_hosts.getLength();
        for (int j = 0; j < numhosts; ++j)
        {
          std::string trial_host = trial_hosts[j];
          // Does the start of the host name match and there is a value for the setting?
          if (boost::algorithm::istarts_with(theHost, trial_host) &&
              override[i].lookupValue(theVariable, theValue))
            return true;
        }
      }
    }

    // use default setting instead
    return theConfig.lookupValue(theVariable, theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Error trying to find setting value")
        .addParameter("variable", theVariable);
  }
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

RepoManager::~RepoManager()
{
  itsExpirationThread.interrupt();
  itsMonitorThread.interrupt();
  itsExpirationThread.join();
  itsMonitorThread.join();
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
      updateTasks(new Fmi::AsyncTaskGroup),
      itsMaxThreadCount(10),  // default if not configured
      itsThreadCount(0),
      itsShutdownRequested(false)
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

      // Options

      const std::string& hostname = boost::asio::ip::host_name();

      lookupHostSetting(itsConfig, itsMaxThreadCount, "maxthreads", hostname);
      lookupHostSetting(itsConfig, itsVerbose, "verbose", hostname);
      lookupHostSetting(itsConfig, itsValidPointsCacheDir, "valid_points_cache_dir", hostname);

      itsRepo.verbose(itsVerbose);

      // Phase 1: Establish producer setting

      if (!itsConfig.exists("producers"))
        throw Fmi::Exception(BCP, "Configuration file must specify the producers");

      const libconfig::Setting& prods = itsConfig.lookup("producers");

      if (!prods.isArray())
        throw Fmi::Exception(BCP, "Configured value of 'producers' must be an array");

      // Phase 2: Parse individual producer settings

      if (prods.getLength() == 0)
        std::cerr << (Spine::log_time_str() + ANSI_FG_YELLOW + " [querydata] producer list empty" +
                      ANSI_FG_DEFAULT)
                  << std::endl;

      for (int i = 0; i < prods.getLength(); ++i)
      {
        Producer prod = prods[i];

        if (!itsConfig.exists(prod))
          throw Fmi::Exception(BCP, "Producer settings for " + prod + " are missing");

        ProducerConfig pinfo = parse_producerinfo(prod, itsConfig.lookup(prod));

        // Save the info

        itsConfigList.push_back(pinfo);
      }

      this->configModTime = modtime;

      updateTasks->on_task_error([this](const std::string&) {
          Fmi::Exception::Trace(BCP, "Operation failed").printError();});
    }
    catch (const libconfig::ParseException& e)
    {
      throw Fmi::Exception(BCP,
                             "Qengine configuration " + configfile + " error '" +
                                 std::string(e.getError()) + "' on line " +
                                 std::to_string(e.getLine()));
    }
    catch (const libconfig::ConfigException& e)
    {
      throw Fmi::Exception(BCP, configfile + ": " + std::strerror(ec.value()));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    for (const auto& pinfo : itsConfigList)
    {
      // Note: watcher indexes start from 0, so we can index the producer
      // with a vector to find out which producer the callback instructs to update.

      if (!boost::filesystem::exists(pinfo.directory))
        std::cerr << (Spine::log_time_str() + ANSI_FG_RED + " [querydata] Producer '" +
                      pinfo.producer + "' path '" + pinfo.directory.string() + "' is missing" +
                      ANSI_FG_DEFAULT)
                  << std::endl;

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
    itsExpirationThread = boost::thread(boost::bind(&RepoManager::expirationLoop, this));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Data expiration loop
 */
// ----------------------------------------------------------------------

void RepoManager::expirationLoop()
{
  while (!itsShutdownRequested)
  {
    // Wait 30 seconds. TODO: use condition variable
    for (int i = 0; i < 10 * 30 && !itsShutdownRequested; i++)
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    if (itsShutdownRequested)
      break;

    for (const ProducerConfig& config : itsConfigList)
    {
      if (config.max_age > 0)
      {
        Spine::WriteLock lock(itsMutex);
        itsRepo.expire(config.producer, config.max_age);
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set an old manager to be used during initialization
 */
// ----------------------------------------------------------------------

void RepoManager::setOldManager(boost::shared_ptr<RepoManager> oldmanager)
{
  itsOldRepoManager = std::move(oldmanager);
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove old manager from use once init is complete
 */
// ----------------------------------------------------------------------

void RepoManager::removeOldManager()
{
  itsOldRepoManager.reset();
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

    if (itsMonitorThread.joinable()) {
        itsMonitorThread.join();
    }

    if (itsExpirationThread.joinable()) {
        itsExpirationThread.join();
    }

    updateTasks->stop();
    updateTasks->wait();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    for (const auto& it : itsProducerMap)
    {
      if (it.second == producer)
        return it.first;
    }

    throw Fmi::Exception(BCP, "Request for unknown producer!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
                        const boost::filesystem::path& dir,
                        const boost::regex& /* pattern */,
                        const std::string& message)
{
  try
  {
    std::cout << ANSI_FG_RED << "Error in directory " << dir << " : " << message << ANSI_FG_DEFAULT
              << std::endl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
                         const boost::filesystem::path& /* dir */,
                         const boost::regex& /* pattern */,
                         const Fmi::DirectoryMonitor::Status& status)
{
  try
  {
    const Producer& producer = itsProducerMap.find(id)->second;

    // Collect names of files to be unloaded or loaded

    Files removals;
    Files additions;
    for (const auto& file_status : *status)
    {
      if (file_status.second == Fmi::DirectoryMonitor::DELETE ||
          file_status.second == Fmi::DirectoryMonitor::MODIFY)
      {
        removals.push_back(file_status.first);
      }

      if (file_status.second == Fmi::DirectoryMonitor::CREATE ||
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
      for (const auto& file : removals)
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
    ++itsThreadCount;

    // Handle new or modified files

#if 0
	std::cerr << ANSI_FG_GREEN << "Threads: " << itsThreadCount
			  << " " << filename << ANSI_FG_DEFAULT << std::endl;
#endif
    updateTasks->handle_finished();
    updateTasks->add("RepoManager::load", std::bind(&RepoManager::load, this, producer, additions));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

void RepoManager::load(Producer producer,
                       Files files)  // NOLINT(performance-unnecessary-value-param)
{
  if (itsShutdownRequested)
  {
    --itsThreadCount;
    return;
  }

  // We expect timestamps and want the newest file first
  std::sort(files.rbegin(), files.rend());

  const ProducerConfig& conf = producerConfig(producer);

  // Try establishing old config
  boost::optional<ProducerConfig> oldconf;
  try
  {
    if (itsOldRepoManager)
      oldconf = itsOldRepoManager->producerConfig(producer);
  }
  catch (...)
  {
  }

  // Do not use old repo if configuration has changed

  const bool try_old_repo = (oldconf && *oldconf == conf);

  unsigned int successful_loads = 0;

  for (const auto& filename : files)
  {
    // Done if the remaining files would not be accepted for being older
    if (successful_loads >= conf.number_to_keep)
      break;

    // files may be corrupt, hence we catch exceptions
    try
    {
      SharedModel model;

      // Try using the old repo if it is available

      if (try_old_repo)
      {
        Spine::ReadLock lock(itsOldRepoManager->itsMutex);

        // Failure to get old data is not an error here
        try
        {
          model = itsOldRepoManager->itsRepo.getModel(producer, filename);
        }
        catch (...)
        {
        }
      }

      const bool load_new_data = !model;

      // Load directly if the old repo was not useful
      if (load_new_data)
      {
        if (itsVerbose)
          std::cout << Spine::log_time_str() + " QENGINE LOAD " + filename.string() << std::endl;

        model = boost::make_shared<Model>(filename,
                                          itsValidPointsCacheDir,
                                          conf.producer,
                                          conf.leveltype,
                                          conf.isclimatology,
                                          conf.isfullgrid,
                                          conf.isrelativeuv,
                                          conf.update_interval,
                                          conf.minimum_expires,
                                          conf.mmap);
      }

      if (itsVerbose && load_new_data)
      {
        std::ostringstream msg;
        msg << Spine::log_time_str() << " QENGINE ORIGINTIME for " << filename << " is "
            << model->originTime() << " HASH VALUE is " << hash_value(*model) << "\n";

        std::cout << msg.str() << std::flush;
      }

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
      Fmi::Exception exception(BCP, "QEngine failed to load the file!", nullptr);
      exception.addParameter("File", filename.c_str());
      std::cerr << exception.getStackTrace();
    }
  }  // for all files

  --itsThreadCount;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the repositories have been scanned at least once
 */
// ----------------------------------------------------------------------

bool RepoManager::ready() const
{
  return (itsConfigList.empty() || (itsThreadCount == 0 && itsMonitor.ready()));
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

    for (const ProducerConfig& config : itsConfigList)
    {
      if (config.producer == producer)
        return config;
    }

    // NOT REACHED
    throw Fmi::Exception(BCP, "Unknown producer config '" + producer + "' requested");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
