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
#include "ValidPoints.h"
#include <boost/bind/bind.hpp>
#include <filesystem>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <macgyver/FileSystem.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <spine/ConfigTools.h>
#include <spine/Convenience.h>
#include <spine/Exceptions.h>
#include <spine/Reactor.h>
#include <cassert>
#include <memory>
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
  try
  {
    boost::this_thread::disable_interruption do_not_disturb;
    itsExpirationThread.interrupt();
    itsMonitorThread.interrupt();
    itsExpirationThread.join();
    itsMonitorThread.join();
  }
  catch (...)
  {
    std::cout << Fmi::Exception::Trace(BCP, "EXCEPTION IN DESTRUCTOR!") << std::endl;
  }
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
      itsThreadCount(0)
{
  std::error_code ec;

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
      const std::time_t modtime = Fmi::last_write_time(configfile, ec);
      // There is a slight race condition here: time is recorded before the actual config is read
      // If config changes between these two calls, we actually have old timestamp
      // To minimize the effects, modification time is recorded before reading. May cause almost
      // immediate reread if config file is changing rapidly

      // Enable sensible relative include paths
      std::filesystem::path p = configfile;
      p.remove_filename();
      itsConfig.setIncludeDir(p.c_str());

      itsConfig.readFile(configfile.c_str());
      Spine::expandVariables(itsConfig);

      // Options

      int lat_lon_cache_size = 500;
      itsConfig.lookupValue("cache.lat_lon_size", lat_lon_cache_size);
      itsLatLonCache.resize(lat_lon_cache_size);

      const std::string& hostname = boost::asio::ip::host_name();

      lookupHostSetting(itsConfig, itsMaxThreadCount, "maxthreads", hostname);
      lookupHostSetting(itsConfig, itsVerbose, "verbose", hostname);
      lookupHostSetting(itsConfig, itsValidPointsCacheDir, "valid_points_cache_dir", hostname);
      lookupHostSetting(
          itsConfig, itsCleanValidPointsCacheDir, "clean_valid_points_cache_dir", hostname);

      if (itsValidPointsCacheDir.empty())
        std::cerr
            << (Spine::log_time_str() + ANSI_FG_MAGENTA +
                " [querydata] valid_points_cache_dir setting is empty, cache will not be created!" +
                ANSI_FG_DEFAULT)
            << std::endl;

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

      if (!ec)
        this->configModTime = modtime;

      updateTasks->on_task_error([](const std::string& /* unused */)
                                 { Fmi::Exception::Trace(BCP, "Operation failed").printError(); });
    }
    catch (...)
    {
      Spine::Exceptions::handle("Querydata engine");
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
  using namespace boost::placeholders;

  try
  {
    for (const auto& pinfo : itsConfigList)
    {
      // Note: watcher indexes start from 0, so we can index the producer
      // with a vector to find out which producer the callback instructs to update.

      if (!std::filesystem::exists(pinfo.directory))
        std::cerr << (Spine::log_time_str() + ANSI_FG_RED + " [querydata] Producer '" +
                      pinfo.producer + "' path '" + pinfo.directory.string() + "' is missing" +
                      ANSI_FG_DEFAULT)
                  << std::endl;

      auto data_id =
          itsMonitor.watch(pinfo.directory,
                           pinfo.pattern,
                           boost::bind(&RepoManager::update, this, _1, _2, _3, _4),
                           boost::bind(&RepoManager::error, this, _1, _2, _3, _4),
                           pinfo.refresh_interval_secs,
                           Fmi::DirectoryMonitor::CREATE | Fmi::DirectoryMonitor::DELETE |
                               Fmi::DirectoryMonitor::SCAN);

      // Save the info

      itsRepo.add(pinfo);
      itsProducerList.push_back(pinfo.producer);
      itsProducerMap.insert(ProducerMap::value_type(data_id, pinfo.producer));
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
  while (!Spine::Reactor::isShuttingDown())
  {
    // Wait 30 seconds. TODO: use condition variable
    for (int i = 0; i < 10 * 30 && !Spine::Reactor::isShuttingDown(); i++)
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    if (Spine::Reactor::isShuttingDown())
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

void RepoManager::setOldManager(std::shared_ptr<RepoManager> oldmanager)
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
    std::cout << "  -- Shutdown requested (RepoManager)\n";
    itsMonitor.stop();

    if (itsMonitorThread.joinable())
      itsMonitorThread.join();

    if (itsExpirationThread.joinable())
      itsExpirationThread.join();

    updateTasks->stop();
    updateTasks->wait();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
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
                        const std::filesystem::path& dir,
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
                         const std::filesystem::path& /* dir */,
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
      if (file_status.second == Fmi::DirectoryMonitor::SCAN)
      {
        const ProducerConfig& conf = producerConfig(producer);
        auto scan_time = Fmi::SecondClock::universal_time();
        auto next_scan_time = (scan_time + Fmi::Seconds(conf.refresh_interval_secs));

        Spine::WriteLock lock(itsMutex);
        itsRepo.updateProducerStatus(producer, scan_time, next_scan_time);
      }

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

    if (removals.empty() && additions.empty())
    {
      // Nothing to update
      return;
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
    while (!ok && !Spine::Reactor::isShuttingDown())
    {
      {
        if (itsThreadCount <= itsMaxThreadCount)
          ok = true;
      }
      if (!ok)
        boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
    }

    // Abort if there is a shut down request
    if (Spine::Reactor::isShuttingDown())
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
  if (Spine::Reactor::isShuttingDown())
  {
    --itsThreadCount;
    return;
  }

  // We expect timestamps and want the newest file first
  std::sort(files.rbegin(), files.rend());

  const ProducerConfig& conf = producerConfig(producer);

  // Try establishing old config
  std::optional<ProducerConfig> oldconf;
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
  Fmi::DateTime data_load_time(Fmi::DateTime::NOT_A_DATE_TIME);

  for (const auto& filename : files)
  {
    if (Spine::Reactor::isShuttingDown())
      break;

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

        model = Model::create(filename,
                              itsValidPointsCacheDir,
                              conf.producer,
                              conf.leveltype,
                              conf.isclimatology,
                              conf.isfullgrid,
                              conf.isstaticgrid,
                              conf.isrelativeuv,
                              conf.update_interval,
                              conf.minimum_expires,
                              conf.mmap);

        data_load_time = Fmi::SecondClock::universal_time();
      }

      if (itsVerbose && load_new_data)
      {
        std::ostringstream msg;
        msg << Spine::log_time_str() << " QENGINE ORIGINTIME for " << filename << " is "
            << model->originTime() << " HASH VALUE is " << hash_value(*model) << "\n";

        std::cout << msg.str() << std::flush;
      }

      // Update latlon-cache if necessary. In any case make sure model cache is up to date
      // WARNING: DEPRECATED CODE BLOCK IN WGS84 MODE - THE RETURNED SHARED_PTR IS EMPTY

      auto hash = model->gridHashValue();
      auto latlons = itsLatLonCache.find(hash);  // cached coordinates, if any
      if (!latlons)
        itsLatLonCache.insert(hash, model->makeLatLonCache());  // request latlons and cache them
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
      if (Spine::Reactor::isShuttingDown())
        break;

      Fmi::Exception exception(BCP, "QEngine failed to load the file!", nullptr);
      exception.addParameter("File", filename.c_str());
      std::cerr << exception.getStackTrace();
    }
  }  // for all files

  if (!Spine::Reactor::isShuttingDown())
  {
    Spine::WriteLock lock(itsMutex);
    itsRepo.updateProducerStatus(producer, data_load_time, itsRepo.getAllModels(producer).size());
  }

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

void RepoManager::cleanValidPointsCache()
{
  if (!std::filesystem::exists(itsValidPointsCacheDir) ||
      !std::filesystem::is_directory(itsValidPointsCacheDir))
    return;

  std::set<std::string> cachefiles;
  for (const auto& producer : itsProducerList)
  {
    const auto shared_models = itsRepo.getAllModels(producer);
    for (const auto& shared_model : shared_models)
    {
      if (shared_model.second->itsValidPoints)
        cachefiles.insert(shared_model.second->itsValidPoints->cacheFile());
    }
  }

  if (cachefiles.empty())
    return;

  // boost::system::error_code ec;
  std::filesystem::directory_iterator end_itr;
  for (std::filesystem::directory_iterator itr(itsValidPointsCacheDir); itr != end_itr; ++itr)
  {
    if (SmartMet::Spine::Reactor::isShuttingDown())
      return;

    if (is_regular_file(itr->status()))
    {
      std::string filename = (itsValidPointsCacheDir + "/" + itr->path().filename().string());
      if (cachefiles.find(filename) == cachefiles.end())
      {
        if (itsCleanValidPointsCacheDir)
        {
          std::cerr << (Spine::log_time_str() + ANSI_FG_MAGENTA +
                        " [querydata] Deleting redundant valid points cache file '" + filename +
                        "'" + ANSI_FG_DEFAULT)
                    << std::endl;
          std::filesystem::remove(filename);
        }
        else
        {
          std::cerr << (Spine::log_time_str() + ANSI_FG_MAGENTA +
                        " [querydata] Redundant valid points cache file detected '" + filename +
                        "', consider deleting it!" + ANSI_FG_DEFAULT)
                    << std::endl;
        }
      }
    }
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
