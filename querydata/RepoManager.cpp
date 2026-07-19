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
#include "RadarCache.h"
#include "RadarReader.h"
#include "Repository.h"
#include <boost/bind/bind.hpp>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <macgyver/FileSystem.h>
#include <macgyver/StringConversion.h>
#include <macgyver/ThreadName.h>
#include <macgyver/TypeName.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <spine/ConfigTools.h>
#include <spine/Convenience.h>
#include <spine/Exceptions.h>
#include <spine/Reactor.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
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
// Path of the decoded scratch .sqd for a radar source frame. Deterministic
// (source stem + modification time) so a re-scan does not reconvert.
std::filesystem::path radarScratchPath(const std::filesystem::path& scratchdir,
                                       const Producer& producer,
                                       const std::filesystem::path& source)
{
  std::error_code ec;
  auto mtime = std::filesystem::last_write_time(source, ec);
  const auto stamp = ec ? 0LL : static_cast<long long>(mtime.time_since_epoch().count());
  const std::filesystem::path dir = scratchdir / producer;
  return dir / (source.stem().string() + "_" + std::to_string(stamp) + ".sqd");
}

// Decode a radar GeoTIFF/ODIM source frame into the scratch .sqd if it is not
// already present and current. Returns the scratch path. Writes atomically via
// a temporary file + rename so a concurrent reader never sees a partial file.
std::filesystem::path convertRadarToScratch(const std::filesystem::path& scratchdir,
                                            const Producer& producer,
                                            const std::filesystem::path& source,
                                            RadarFormat format)
{
  const std::filesystem::path scratch = radarScratchPath(scratchdir, producer, source);

  std::error_code ec;
  if (std::filesystem::exists(scratch, ec))
    return scratch;  // name embeds the source mtime, so an existing file is current

  std::filesystem::create_directories(scratch.parent_path(), ec);

  auto data = readRadarFile(source, format);
  if (!data)
    throw Fmi::Exception(BCP, "Radar decode produced no data: " + source.string());

  // Write to a dot-prefixed temp name in the same directory (same filesystem, so
  // the rename below is atomic). Leading-dot files are automatically ignored by
  // the newbase querydata reader and by the directory scans, so a half-written
  // temp left by a crash is never picked up as data.
  const std::filesystem::path tmp =
      scratch.parent_path() / ("." + scratch.filename().string() + "." +
                               std::to_string(reinterpret_cast<std::uintptr_t>(data.get())));
  {
    std::ofstream out(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out)
      throw Fmi::Exception(BCP, "Cannot open radar scratch file for writing: " + tmp.string());
    out << *data;
    if (!out)
      throw Fmi::Exception(BCP, "Failed to write radar scratch file: " + tmp.string());
  }
  std::filesystem::rename(tmp, scratch, ec);
  if (ec)
  {
    std::filesystem::remove(tmp, ec);
    throw Fmi::Exception(BCP,
                         "Failed to rename radar scratch file into place: " + scratch.string());
  }
  return scratch;
}
}  // namespace

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
    std::cout << Fmi::Exception::Trace(BCP, "EXCEPTION IN DESTRUCTOR!") << '\n';
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
      itsRepo.verbose(itsVerbose);

      // Directory for decoded radar (GeoTIFF/ODIM) scratch .sqd files. Should be
      // on a real disk volume so the kernel page cache manages the mapped frames.
      std::string radar_scratch_dir;
      if (itsConfig.lookupValue("radar.scratch_directory", radar_scratch_dir) &&
          !radar_scratch_dir.empty())
        itsRadarScratchDir = radar_scratch_dir;

      // Byte budget for the radar scratch cache; 0 (default) = unlimited.
      long long radar_cache_size = 0;
      if (itsConfig.lookupValue("radar.cache_size", radar_cache_size) && radar_cache_size > 0)
        itsRadarCacheBytes = static_cast<std::uintmax_t>(radar_cache_size);

      // Idle timeout (seconds) after which an untouched lazy producer is
      // unloaded; 0 (default) = never unload by idle.
      int radar_idle_timeout = 0;
      if (itsConfig.lookupValue("radar.idle_timeout", radar_idle_timeout) && radar_idle_timeout > 0)
        itsRadarIdleTimeout = static_cast<unsigned int>(radar_idle_timeout);

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
                  << '\n';

      for (int i = 0; i < prods.getLength(); ++i)
      {
        Producer prod = prods[i];

        if (!itsConfig.exists(prod))
          throw Fmi::Exception(BCP, "Producer settings for " + prod + " are missing");

        ProducerConfig pinfo = parse_producerinfo(prod, itsConfig.lookup(prod));

        // Save the info

        if (pinfo.islazy)
          itsLazyProducers.insert(pinfo.producer);
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
 * \brief Reconcile the radar scratch cache at startup
 *
 * Decoded radar frames live under itsRadarScratchDir in per-source (producer)
 * subdirs. Rather than wiping everything (which would discard a warm working
 * set that is expensive to rebuild), this reconciles the on-disk cache against
 * the configured sources: it removes crash residue (dot-prefixed temps, .trash),
 * drops frames whose source has rotated away and sources no longer configured,
 * keeps still-current frames so a restart is warm, and enforces the
 * radar.cache_size budget by group-LRU eviction of whole sources. See RadarCache.
 *
 * Called once per process before init() starts scanning; not on a config
 * hot-reload, where the previous RepoManager still owns live scratch.
 */
// ----------------------------------------------------------------------

void RepoManager::reconcileRadarCache() const
{
  try
  {
    RadarCache cache(itsRadarScratchDir, itsRadarCacheBytes);

    // Map a source id (= producer subdir) to its configured source directory.
    std::map<std::string, std::filesystem::path> sourceDirs;
    for (const ProducerConfig& config : itsConfigList)
      sourceDirs[config.producer] = config.directory;

    // keepFile: a cached file is current iff a source frame still maps to it
    // (same stem + mtime). The set of valid scratch names per producer is
    // computed lazily from a one-time source-directory scan.
    auto validNames = std::make_shared<std::map<std::string, std::set<std::string>>>();
    auto keepFile = [this, sourceDirs, validNames](const std::string& sourceId,
                                                   const std::string& filename) -> bool
    {
      auto dit = sourceDirs.find(sourceId);
      if (dit == sourceDirs.end())
        return false;  // producer no longer configured -> drop
      auto vit = validNames->find(sourceId);
      if (vit == validNames->end())
      {
        std::set<std::string> names;
        std::error_code ec;
        for (std::filesystem::directory_iterator it(dit->second, ec), end; it != end && !ec;
             it.increment(ec))
        {
          if (it->is_regular_file(ec))
            names.insert(
                radarScratchPath(itsRadarScratchDir, sourceId, it->path()).filename().string());
        }
        vit = validNames->emplace(sourceId, std::move(names)).first;
      }
      return vit->second.count(filename) > 0;
    };

    // pinned: never evict a configured source (it would just be re-decoded on the
    // eager load that follows) or one that already has live models.
    auto pinned = [this, sourceDirs](const std::string& sourceId) -> bool
    {
      if (sourceDirs.count(sourceId) > 0)
        return true;
      return !itsRepo.getAllModels(sourceId).empty();
    };

    cache.reconcile(keepFile, pinned);
  }
  catch (...)
  {
    // Startup reconcile is best effort: never block engine init on it.
    std::cout << Fmi::Exception::Trace(BCP, "Failed to reconcile radar scratch cache") << '\n';
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
                  << '\n';

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

    itsMonitorThread = boost::thread(
        [this]()
        {
          Fmi::set_thread_name("upd-qd-mon");
          itsMonitor.run();
        });
    itsExpirationThread = boost::thread(
        [this]()
        {
          Fmi::set_thread_name("upd-qd-exp");
          expirationLoop();
        });
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

    // Unload cold lazy radar producers (idle timeout + size budget) and reclaim
    // orphaned scratch. This is what makes radar.cache_size bind on the working
    // set: the least-recently-accessed loaded producer is unloaded until the
    // scratch cache is under budget.
    sweepLazyProducers();
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
              << '\n';
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
			  << " " << filename << ANSI_FG_DEFAULT << '\n';
#endif
    updateTasks->handle_finished();
    updateTasks->add("upd-qd", std::bind(&RepoManager::load, this, producer, additions));
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

void RepoManager::load(Producer producer,  // NOLINT(performance-unnecessary-value-param)
                       Files files)        // NOLINT(performance-unnecessary-value-param)
{
  if (Spine::Reactor::isShuttingDown())
  {
    --itsThreadCount;
    return;
  }

  // We expect timestamps and want the newest file first
  std::sort(files.rbegin(), files.rend());

  const ProducerConfig& conf = producerConfig(producer);

  // Lazy radar producer: catalogue the full time dimension (header-only, no pixel
  // decode) so GetCapabilities can advertise every frame regardless of what is
  // decoded.
  if (conf.islazy)
  {
    itsRadarCatalog.update(conf.producer, files);
    // A cold lazy producer is catalogue-only: its frames are decoded on first
    // access (ensureLoaded). A hot one (already loaded) keeps being refreshed
    // here so a live animation stays current.
    if (!producerHasModels(producer))
    {
      --itsThreadCount;
      return;
    }
  }

  loadModels(producer, files, conf);

  --itsThreadCount;
}

// ----------------------------------------------------------------------
/*!
 * \brief Decode files into models and add them to the repository
 *
 * The core of load(): shared by the directory-monitor path and by on-access
 * lazy loading (ensureLoaded). Loads the newest number_to_keep files. Does not
 * touch itsThreadCount (the caller owns it) and expects files sorted newest
 * first.
 */
// ----------------------------------------------------------------------

void RepoManager::loadModels(const Producer& producer,
                             const Files& files,
                             const ProducerConfig& conf)
{
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
          std::cout << Spine::log_time_str() + " QENGINE LOAD " + filename.string() << '\n';

        const RadarFormat radarformat = detectRadarFormat(filename);
        if (radarformat == RadarFormat::GeoTiff || radarformat == RadarFormat::Odim)
        {
          // Decode the radar frame into a scratch .sqd, then load it
          // memory-mapped exactly like ordinary querydata. The model owns and
          // deletes the scratch on eviction/expiry; identity (path, hash,
          // modification time) stays with the source frame.
          auto scratch =
              convertRadarToScratch(itsRadarScratchDir, conf.producer, filename, radarformat);
          // Mark the source as recently accessed for the cache's group-LRU.
          RadarCache(itsRadarScratchDir, itsRadarCacheBytes).markAccessed(conf.producer);
          model = Model::create(filename,
                                scratch,
                                conf.producer,
                                conf.leveltype,
                                conf.isclimatology,
                                conf.isfullgrid,
                                conf.isstaticgrid,
                                conf.isrelativeuv,
                                conf.update_interval,
                                conf.minimum_expires,
                                conf.mmap,
                                /*ownsdatafile=*/true);
        }
        else
        {
          model = Model::create(filename,
                                conf.producer,
                                conf.leveltype,
                                conf.isclimatology,
                                conf.isfullgrid,
                                conf.isstaticgrid,
                                conf.isrelativeuv,
                                conf.update_interval,
                                conf.minimum_expires,
                                conf.mmap);
        }

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
}

bool RepoManager::producerHasModels(const Producer& producer) const
{
  Spine::ReadLock lock(itsMutex);
  return !itsRepo.getAllModels(producer).empty();
}

std::shared_ptr<std::mutex> RepoManager::lazyLoadMutex(const Producer& producer)
{
  std::lock_guard<std::mutex> guard(itsLazyLoadMapMutex);
  auto& m = itsLazyLoadMutexes[producer];
  if (!m)
    m = std::make_shared<std::mutex>();
  return m;
}

// ----------------------------------------------------------------------
/*!
 * \brief Ensure a lazy producer's servable window is decoded on first access
 *
 * No-op for non-lazy producers (fast set check) and for lazy producers already
 * loaded. On a miss it serialises per producer, re-checks, and decodes the
 * newest number_to_keep catalogued frames into the repository so a subsequent
 * (possibly multifile) get() sees the complete servable window. It takes no lock
 * across the decode except its own per-producer mutex; loadModels locks itsMutex
 * internally, so this must run before the caller takes itsMutex for the get().
 */
// ----------------------------------------------------------------------

void RepoManager::ensureLoaded(const Producer& producer)
{
  try
  {
    // Fast path: only lazy producers are ever loaded on demand.
    if (itsLazyProducers.find(producer) == itsLazyProducers.end())
      return;

    // Record the access time (for idle + size-based unloading), also on the
    // already-loaded hot path so an actively-queried producer stays hot.
    {
      std::lock_guard<std::mutex> g(itsLazyLoadMapMutex);
      itsLazyLastAccess[producer] = std::chrono::steady_clock::now();
    }

    if (producerHasModels(producer))
      return;

    // Serialise concurrent first-access decodes of the same producer.
    auto lk = lazyLoadMutex(producer);
    std::lock_guard<std::mutex> guard(*lk);
    if (producerHasModels(producer))  // another thread just loaded it
      return;

    const ProducerConfig& conf = producerConfig(producer);

    // Decode the servable window: all catalogued frames (loadModels keeps the
    // newest number_to_keep). Sorted newest first to match load().
    Files files;
    for (const auto& frame : itsRadarCatalog.frames(producer))
      files.push_back(frame.path);
    if (files.empty())
      return;
    std::sort(files.rbegin(), files.rend());

    loadModels(producer, files, conf);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Lazy load failed for producer " + producer);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Drop all models of a producer, freeing its memory and (radar) scratch
 *
 * A live Q keeps its model alive until the request finishes, so unloading during
 * a request is safe; the scratch .sqd is deleted when the last reference drops.
 * The producer will be re-decoded on its next access via ensureLoaded.
 */
// ----------------------------------------------------------------------

void RepoManager::unloadProducer(const Producer& producer)
{
  try
  {
    Spine::WriteLock lock(itsMutex);
    itsRepo.resize(producer, 0);
  }
  catch (...)
  {
    // Producer may have no models (race with expiry); nothing to unload.
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Unload cold lazy producers (idle timeout + size budget)
 *
 * Two pressures: a lazy producer untouched for radar.idle_timeout is unloaded to
 * free memory; and while the scratch cache exceeds radar.cache_size, the
 * least-recently-accessed loaded lazy producer is unloaded so its scratch is
 * reclaimed - this is what makes the byte budget actually bind on the working
 * set. Finally the cache reclaims any de-configured / orphaned scratch.
 */
// ----------------------------------------------------------------------

void RepoManager::sweepLazyProducers()
{
  if (itsLazyProducers.empty())
    return;

  const auto now = std::chrono::steady_clock::now();

  std::map<Producer, std::chrono::steady_clock::time_point> access;
  {
    std::lock_guard<std::mutex> g(itsLazyLoadMapMutex);
    access = itsLazyLastAccess;
  }

  auto idleSeconds = [&](const Producer& p) -> long long
  {
    auto it = access.find(p);
    if (it == access.end())
      return std::numeric_limits<long long>::max();  // never accessed -> maximally idle
    return std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
  };

  // 1. Idle unload.
  if (itsRadarIdleTimeout > 0)
  {
    for (const auto& p : itsLazyProducers)
      if (idleSeconds(p) >= static_cast<long long>(itsRadarIdleTimeout) && producerHasModels(p))
        unloadProducer(p);
  }

  // 2. Size-based unload: bound the working set to the byte budget by unloading
  //    the least-recently-accessed loaded lazy producer until under budget.
  if (itsRadarCacheBytes > 0)
  {
    RadarCache cache(itsRadarScratchDir, itsRadarCacheBytes);
    while (cache.totalBytes() > itsRadarCacheBytes)
    {
      Producer victim;
      long long victimIdle = -1;
      for (const auto& p : itsLazyProducers)
      {
        if (!producerHasModels(p))
          continue;
        const long long idle = idleSeconds(p);
        if (idle > victimIdle)  // most idle (oldest access) wins
        {
          victimIdle = idle;
          victim = p;
        }
      }
      if (victimIdle < 0)
        break;  // nothing loaded left to unload
      unloadProducer(victim);
      access.erase(victim);  // do not reconsider (its models are gone)
    }
  }

  // 3. Reclaim de-configured / orphaned scratch.
  if (itsRadarCacheBytes > 0)
  {
    auto pinned = [this](const std::string& id) { return !itsRepo.getAllModels(id).empty(); };
    RadarCache(itsRadarScratchDir, itsRadarCacheBytes).enforceBudget(pinned);
  }
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
