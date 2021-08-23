// ======================================================================
/*!
 * \brief Manage access to the repository
 */
// ======================================================================

#pragma once

#include "Repository.h"
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <macgyver/AsyncTaskGroup.h>
#include <macgyver/Cache.h>
#include <macgyver/DirectoryMonitor.h>
#include <spine/Thread.h>
#include <memory>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
// Collection of files
using Files = std::vector<boost::filesystem::path>;

struct RepoManager
{
  // construction & destruction

  ~RepoManager();
  RepoManager() = delete;
  RepoManager(const std::string& configfile);

  // generic API

  const ProducerConfig& producerConfig(const Producer& producer) const;

  // callback requests

  void update(Fmi::DirectoryMonitor::Watcher id,
              const boost::filesystem::path& dir,
              const boost::regex& pattern,
              const Fmi::DirectoryMonitor::Status& status);

  void error(Fmi::DirectoryMonitor::Watcher id,
             const boost::filesystem::path& dir,
             const boost::regex& pattern,
             const std::string& message);

  void init();
  bool ready() const;
  void shutdown();

  // data members

  mutable Spine::MutexType itsMutex;  // mutexes should always be mutable
  libconfig::Config itsConfig;
  bool itsVerbose;

  Fmi::DirectoryMonitor itsMonitor;
  boost::thread itsMonitorThread;
  boost::thread itsExpirationThread;
  std::unique_ptr<Fmi::AsyncTaskGroup> updateTasks;

  // info on producers generated by constructor

  using ProducerConfigList = std::list<ProducerConfig>;
  ProducerConfigList itsConfigList;

  // available producers

  ProducerList itsProducerList;

  // watchers and the respective producer

  using ProducerMap = std::map<Fmi::DirectoryMonitor::Watcher, Producer>;
  ProducerMap itsProducerMap;

  // loaded data, updated regularly

  Repository itsRepo;

  std::time_t configModTime;  // Timestamp of configuration file loaded
  inline std::time_t getConfigModTime() const { return configModTime; }

  void setOldManager(boost::shared_ptr<RepoManager> oldmanager);
  void removeOldManager();

 private:
  void load(Producer producer, Files files);
  void expirationLoop();

  Fmi::DirectoryMonitor::Watcher id(const Producer& producer) const;

  int itsMaxThreadCount;
  boost::atomic<int> itsThreadCount;

  using LatLonCache = Fmi::Cache::Cache<std::size_t, boost::shared_ptr<std::vector<NFmiPoint>>>;
  LatLonCache itsLatLonCache;

  std::string itsValidPointsCacheDir = "/var/smartmet/querydata/validpoints";

  boost::shared_ptr<RepoManager> itsOldRepoManager;
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
