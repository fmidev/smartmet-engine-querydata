// ======================================================================
/*!
 * \brief Interface of Synchronizer class
 */
// ======================================================================

#pragma once

#include <spine/Thread.h>
#include <boost/asio.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/array.hpp>
#include <boost/optional.hpp>

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
  SyncGroup();

  SyncGroup(const ProducerMap& theMap);

  void setBaseline(const ProducerMap& theUpdate);

  void update(const ProducerMap& theUpdate);

  ProducerMap getConsensus() const;

 private:
  ProducerMap itsConsensus;
};

class SynchronizerConfig : public SmartMet::Spine::ConfigBase
{
 public:
  ~SynchronizerConfig();

  SynchronizerConfig(const std::string& configFile);

  bool parse();

  unsigned short getPort() const;

  std::string getHostName() const;

  std::string getFailedReason() const;

 private:
  std::string itsHostName;

  unsigned short itsPort;

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

  void launch(SmartMet::Spine::Reactor* theReactor);

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

  unsigned short itsPort;

  ba::io_service itsIoService;

  ba::ip::udp::socket itsSocket;

  ba::ip::udp::endpoint itsRemoteEnd;

  ba::ip::udp::endpoint itsSenderEnd;

  ba::deadline_timer itsTimer;

  SmartMet::Spine::Reactor* itsReactor;

  bool hasLaunched;

  bool isLaunchable;

  boost::array<char, 32768> itsSocketBuffer;

  boost::scoped_ptr<boost::thread> itsCommThread;

  std::string itsSendBuffer;

  SmartMet::Spine::MutexType itsMutex;

  std::map<std::string, SyncGroup> itsSyncGroups;

  std::list<PendingUpdate> itsPendingUpdates;
};

}  // namspace Q
}  // namspace Engine
}  // namspace SmartMet
