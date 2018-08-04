// ======================================================================
/*!
 * \brief Interface of Synchronizer class
 */
// ======================================================================

#pragma once

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <spine/Thread.h>

#include <memory>
#include <string>

#include <spine/ConfigBase.h>

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

typedef std::map<std::string, std::vector<bp::ptime> > ProducerMap;

struct PendingUpdate
{
  bp::ptime timestamp;

  std::list<std::string> handlers;

  ProducerMap producers;
};

class SyncGroup
{
 public:
  SyncGroup() = default;

  SyncGroup(const ProducerMap& theMap);

  void setBaseline(const ProducerMap& theUpdate);

  void update(const ProducerMap& theUpdate);

  ProducerMap getConsensus() const;

 private:
  ProducerMap itsConsensus;
};

class SynchronizerConfig : public Spine::ConfigBase
{
 public:
  ~SynchronizerConfig() = default;

  SynchronizerConfig(const std::string& configFile);

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

  boost::optional<ProducerMap> getSynchedData(const std::string& syncGroup);

  boost::optional<std::vector<bp::ptime> > getSynchedData(const std::string& syncGroup,
                                                          const std::string& producer);

  void launch(Spine::Reactor* theReactor);

  void shutdown();
  void shutdownRequestFlagSet();

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

  Spine::Reactor* itsReactor;

  bool hasLaunched = false;

  bool isLaunchable = false;

  boost::array<char, 32768> itsSocketBuffer;

  std::unique_ptr<boost::thread> itsCommThread;

  std::string itsSendBuffer;

  Spine::MutexType itsMutex;

  std::map<std::string, SyncGroup> itsSyncGroups;

  std::list<PendingUpdate> itsPendingUpdates;
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
