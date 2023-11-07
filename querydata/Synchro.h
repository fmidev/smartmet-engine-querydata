// ======================================================================
/*!
 * \brief Interface of Synchronizer class
 */
// ======================================================================

#pragma once

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/optional.hpp>
#include <spine/ConfigBase.h>
#include <spine/Thread.h>
#include <memory>
#include <string>

namespace SmartMet
{
class QueryDataMessage;

namespace Spine
{
class Reactor;
}

namespace Engine
{
namespace Querydata
{
class Engine;

namespace ba = boost::asio;
namespace bp = boost::posix_time;

using ProducerMap = std::map<std::string, std::vector<Fmi::DateTime>>;

struct PendingUpdate
{
  Fmi::DateTime timestamp;

  std::list<std::string> handlers;

  ProducerMap producers;
};

class SyncGroup
{
 public:
  SyncGroup() = default;

  explicit SyncGroup(ProducerMap theMap);

  void setBaseline(const ProducerMap& theUpdate);

  void update(const ProducerMap& theUpdate);

  ProducerMap getConsensus() const;

 private:
  ProducerMap itsConsensus;
};

class SynchronizerConfig : public Spine::ConfigBase
{
 public:
  ~SynchronizerConfig() override = default;
  explicit SynchronizerConfig(const std::string& configFile);

  SynchronizerConfig() = delete;
  SynchronizerConfig(const SynchronizerConfig& other) = delete;
  SynchronizerConfig& operator=(const SynchronizerConfig& other) = delete;
  SynchronizerConfig(SynchronizerConfig&& other) = delete;
  SynchronizerConfig& operator=(SynchronizerConfig&& other) = delete;

  bool parse();

  unsigned short getPort() const;

  std::string getHostName() const;

  std::string getFailedReason() const;

 private:
  std::string itsHostName;

  unsigned short itsPort = 0;

  std::string itsFailedReason;
};

class Synchronizer
{
 public:
  Synchronizer(SmartMet::Engine::Querydata::Engine* itsParent, const std::string& configFile);

  ~Synchronizer();

  Synchronizer() = delete;
  Synchronizer(const Synchronizer& other) = delete;
  Synchronizer& operator=(const Synchronizer& other) = delete;
  Synchronizer(Synchronizer&& other) = delete;
  Synchronizer& operator=(Synchronizer&& other) = delete;

  boost::optional<ProducerMap> getSynchedData(const std::string& syncGroup);

  boost::optional<std::vector<Fmi::DateTime>> getSynchedData(const std::string& syncGroup,
                                                         const std::string& producer);

  void launch(Spine::Reactor* theReactor);

  void shutdown();

 private:
  void start_receive();

  void handle_receive(const boost::system::error_code& err, std::size_t bytes_transferred);

  void handle_send(const boost::system::error_code& err, std::size_t bytes_transferred);

  void process_message(const QueryDataMessage& incomingMessage);

  void fire_timer(const boost::system::error_code& err);

  void send_broadcast();

  void update_consensus();

  SmartMet::Engine::Querydata::Engine* itsParentEngine;

  SynchronizerConfig itsConfig;

  std::string itsHostName;

  unsigned short itsPort = 0;

  ba::io_service itsIoService;

  ba::ip::udp::socket itsSocket;

  ba::ip::udp::endpoint itsRemoteEnd;

  ba::ip::udp::endpoint itsSenderEnd;

  ba::deadline_timer itsTimer;

  Spine::Reactor* itsReactor = nullptr;

  bool hasLaunched = false;

  bool isLaunchable = false;

  boost::array<char, 32768> itsSocketBuffer;

  boost::movelib::unique_ptr<boost::thread> itsCommThread;

  std::string itsSendBuffer;

  Spine::MutexType itsMutex;

  std::map<std::string, SyncGroup> itsSyncGroups;

  std::list<PendingUpdate> itsPendingUpdates;
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
