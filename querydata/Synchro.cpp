#include "Synchro.h"
#include "Engine.h"
#include "QueryDataMessage.pb.h"
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>
#include <spine/Reactor.h>

#ifndef BROADCAST_TIMER_DELAY
#define BROADCAST_TIMER_DELAY 10
#endif

namespace
{
#if 0
  void dumpMessage(const QueryDataMessage& mesg)
  {
	std::cout << "Host name: " << mesg.name() << std::endl;
	std::cout << "Producer infos :" << std::endl;
	for (int i = 0; i < mesg.prodinfos_size(); ++i)
	  {
		auto info = mesg.prodinfos(i);
		std::cout << info.prodname();
		for (int j = 0; j < info.origintimes_size(); ++j)
		  {
			std::cout << " " << info.origintimes(j);
		  }

		std::cout << std::endl;
	  }

	std::cout << "Handlers: " << std::endl;
	for (int i = 0; i < mesg.handlers_size(); ++i)
	  {
		std::cout << " " << mesg.handlers(i);
	  }
	std::cout << std::endl << std::endl;
  }
#endif

void dumpProducerMap(const SmartMet::Engine::Querydata::ProducerMap& map)
{
  try
  {
    for (auto it = map.begin(); it != map.end(); ++it)
    {
      std::cout << "Producer: " << it->first << std::endl;
      for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
      {
        std::cout << " " << *it2;
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

template <class T>
void printList(const T& theList)
{
  try
  {
    for (auto it = theList.begin(); it != theList.end(); ++it)
    {
      std::cout << *it << std::endl;
    }
    std::cout << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool isNotOld(const boost::posix_time::ptime& target,
              const SmartMet::Engine::Querydata::PendingUpdate& compare)
{
  return compare.timestamp > target;
}

std::string makeRandomString(unsigned int length)
{
  try
  {
    static std::string& charset =
        *new std::string("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");
    static int setSize = boost::numeric_cast<int>(charset.size());
    static boost::random::mt19937 generator(boost::numeric_cast<unsigned int>(std::time(nullptr)));
    static boost::random::uniform_int_distribution<> dist(0, setSize);

    std::string result;
    result.resize(length);

    for (unsigned int i = 0; i < length; ++i)
    {
      result[i] = charset[boost::numeric_cast<std::size_t>(dist(generator))];
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
Synchronizer::Synchronizer(SmartMet::Engine::Querydata::Engine* itsParent,
                           const std::string& configFile)
    : itsParentEngine(itsParent),
      itsConfig(configFile),
      itsHostName(),
      itsPort(),
      itsIoService(),
      itsSocket(itsIoService),
      itsRemoteEnd(),
      itsTimer(itsIoService),
      itsReactor(nullptr),
      hasLaunched(false),
      isLaunchable(true)

{
  try
  {
    bool success = itsConfig.parse();

    if (!success)
    {
      // Config not succesfully parsed, we are not launchable
      isLaunchable = false;
    }
    else
    {
      // We are launchable, setup networking
      itsPort = itsConfig.getPort();

      itsHostName = itsConfig.getHostName();

      // Set socket to broadcast
      itsSocket.open(ba::ip::udp::v4());
      itsSocket.set_option(ba::socket_base::broadcast(true));

      // Bind socket to local endpoint
      ba::ip::udp::endpoint local_endpoint(ba::ip::udp::v4(), itsPort);
      itsSocket.bind(local_endpoint);

      // Set up the remote endpoint for broadcast
      itsRemoteEnd.port(boost::numeric_cast<unsigned short>(itsPort));
      itsRemoteEnd.address(ba::ip::address_v4::broadcast());
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

Synchronizer::~Synchronizer()
{
  try
  {
    itsIoService.stop();
    if (itsCommThread)
      itsCommThread->join();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Synchronizer::launch(Spine::Reactor* theReactor)
{
  try
  {
    Spine::WriteLock lock(itsMutex);  // This lock may be unnecessary
    if (!isLaunchable)
    {
      throw Spine::Exception(
          BCP, "Unable to launch QEngine synchronization, reason: " + itsConfig.getFailedReason());
    }
    if (!hasLaunched)
    {
      hasLaunched = true;
      itsReactor = theReactor;

      // Start broadcast timer loop
      itsTimer.expires_from_now(bp::seconds(0));
      itsTimer.async_wait(boost::bind(&Synchronizer::fire_timer, this, _1));

      // Start thread for async operations
      itsCommThread.reset(new boost::thread(boost::bind(
          static_cast<std::size_t (boost::asio::io_service::*)(void)>(&ba::io_service::run),
          &itsIoService)));

      // Start listening for broadcasts
      start_receive();
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown
 */
// ----------------------------------------------------------------------

void Synchronizer::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (Synchronizer)\n";
    itsIoService.stop();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Synchronizer::shutdownRequestFlagSet()
{
  try
  {
    itsIoService.stop();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::optional<ProducerMap> Synchronizer::getSynchedData(const std::string& handler)
{
  try
  {
    Spine::WriteLock lock(itsMutex);
    if (!hasLaunched)
    {
      // Attempting to get synched data from a node which is not synching
      throw Spine::Exception(BCP,
                             "Attempted to get synched metadata from a non-synching QEngine node");
    }
    auto it = itsSyncGroups.find(handler);
    if (it == itsSyncGroups.end())
    {
      // Unknown handler
      return boost::optional<ProducerMap>();
    }
    else
    {
      return boost::optional<ProducerMap>(it->second.getConsensus());
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::optional<std::vector<bp::ptime> > Synchronizer::getSynchedData(const std::string& handler,
                                                                      const std::string& producer)
{
  try
  {
    Spine::WriteLock lock(itsMutex);
    auto it = itsSyncGroups.find(handler);
    if (it == itsSyncGroups.end())
    {
      // Unknown handler
      return boost::optional<std::vector<bp::ptime> >();
    }
    else
    {
      auto producerMap = it->second.getConsensus();
      auto it2 = producerMap.find(producer);
      if (it2 == producerMap.end())
      {
        // Unknown producer
        return boost::optional<std::vector<bp::ptime> >();
      }
      else
      {
        return boost::optional<std::vector<bp::ptime> >(it2->second);
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Synchronizer::send_broadcast()
{
  try
  {
    // Obtain the local QEngine metadata
    auto metadata = itsParentEngine->getSynchroInfos();

    // New message to be sent
    QueryDataMessage mesg;

    // The host name
    mesg.set_name(itsHostName);

    for (auto iter = metadata.begin(); iter != metadata.end(); ++iter)
    {
      QueryDataMessage::ProducerInfo* thisProducer = mesg.add_prodinfos();

      thisProducer->set_prodname(iter->first);
      for (auto it = iter->second.begin(); it != iter->second.end(); ++it)
      {
        thisProducer->add_origintimes(Fmi::to_iso_string(*it));
      }
    }

    // Add known handlers
    auto uriMap = itsReactor->getURIMap();
    for (auto it = uriMap.begin(); it != uriMap.end(); ++it)
    {
      mesg.add_handlers(it->first);
    }

    // Send message asynchronously
    mesg.SerializeToString(&itsSendBuffer);
#ifndef NDEBUG
    std::cout << "Sending data" << std::endl;
#endif
    itsSocket.async_send_to(ba::buffer(itsSendBuffer),
                            itsRemoteEnd,
                            boost::bind(&Synchronizer::handle_send, this, _1, _2));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Synchronizer::update_consensus()
{
  try
  {
    Spine::WriteLock lock(itsMutex);

    // Clean too old responses from the pending queue
    auto firstValidTime = bp::microsec_clock::universal_time() - bp::seconds(BROADCAST_TIMER_DELAY);
    auto iterator = std::find_if(itsPendingUpdates.begin(),
                                 itsPendingUpdates.end(),
                                 boost::bind(&::isNotOld, firstValidTime, _1));
    itsPendingUpdates.erase(itsPendingUpdates.begin(), iterator);

    auto uriMap = itsReactor->getURIMap();
    // Obtain the local QEngine metadata
    auto metadata = itsParentEngine->getSynchroInfos();

#ifndef NDEBUG
// Print local datas
// std::cout << "Local datas: " << std::endl;
// ::dumpProducerMap(metadata);
#endif

    std::set<std::string> updatedHandlers;

    for (auto it = uriMap.begin(); it != uriMap.end(); ++it)
    {
      itsSyncGroups[it->first].setBaseline(metadata);
      updatedHandlers.insert(it->first);
    }

    // Intersect each pending update with the current data (baseline set previously)
    for (auto& update : itsPendingUpdates)
    {
      for (auto& handler : update.handlers)
      {
        // Find the handler from the current data
        auto it = itsSyncGroups.find(handler);
        if (it == itsSyncGroups.end())
        {
          // Unknown handler, add to list
          itsSyncGroups.insert(std::make_pair(handler, SyncGroup(update.producers)));
        }
        else
        {
          it->second.update(update.producers);
        }

        updatedHandlers.insert(handler);
      }
    }

    // Remove stale synchronization groups

    for (auto it = itsSyncGroups.begin(); it != itsSyncGroups.end();)
    {
      auto findit = updatedHandlers.find(it->first);
      if (findit == updatedHandlers.end())
      {
        // This handler was not updated, consider it stale and remove it
        itsSyncGroups.erase(it++);
        continue;
      }

      ++it;
    }

// Difference is now complete
#ifndef NDEBUG
    //	  Print consensus on some synchro groups ( group == handler)
    auto it = itsSyncGroups.find("/pointforecast");
    if (it != itsSyncGroups.end())
    {
      std::cout << "Sync Group: " << it->first << std::endl;
      ::dumpProducerMap(it->second.getConsensus());
    }
    std::cout << "Consensus calculated using " << itsPendingUpdates.size() << " responses"
              << std::endl;
#endif
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Synchronizer::fire_timer(const boost::system::error_code& err)
{
  try
  {
    if (err != boost::asio::error::operation_aborted)
    {
      // Update local cluser QEngine consensus
      update_consensus();

      // Send QEngine content broadcast
      send_broadcast();

      // Continue broadcast loop
      itsTimer.expires_from_now(bp::seconds(BROADCAST_TIMER_DELAY));
      itsTimer.async_wait(boost::bind(&Synchronizer::fire_timer, this, _1));
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Synchronizer::handle_send(const boost::system::error_code& /* err */,
                               std::size_t bytes_transferred)
{
// Currently does nothing when send has been performed
#ifndef NDEBUG
  std::cout << "Sent " << bytes_transferred << " bytes." << std::endl;
#endif
}

void Synchronizer::start_receive()
{
  try
  {
    itsSocket.async_receive_from(ba::buffer(itsSocketBuffer),
                                 itsSenderEnd,
                                 boost::bind(&Synchronizer::handle_receive,
                                             this,
                                             ba::placeholders::error,
                                             ba::placeholders::bytes_transferred));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Synchronizer::handle_receive(const boost::system::error_code& err,
                                  std::size_t bytes_transferred)
{
  try
  {
    if (!err)
    {
      std::string message_buffer(itsSocketBuffer.begin(), bytes_transferred);
      QueryDataMessage incomingMessage;
      bool success = incomingMessage.ParseFromString(message_buffer);
      if (success)
      {
        // Check message
        process_message(incomingMessage);
      }
      else
      {
        // Failed to parse message, maybe we just skip?
      }
    }
    else
    {
      // Some error occurred
    }

    // Go back to listen the socket
    start_receive();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Synchronizer::process_message(const QueryDataMessage& incomingMessage)
{
  try
  {
    auto name = incomingMessage.name();

    // Ignore messages to self
    if (name != itsHostName)
    {
      // Make producer map from the message

      PendingUpdate theUpdate;

      theUpdate.timestamp = bp::microsec_clock::universal_time();

      for (int i = 0; i < incomingMessage.prodinfos_size(); ++i)
      {
        auto info = incomingMessage.prodinfos(i);

        std::size_t osize = boost::numeric_cast<std::size_t>(info.origintimes_size());

        std::vector<bp::ptime> theseTimes(osize);

        for (std::size_t j = 0; j < osize; ++j)
        {
          theseTimes[j] = bp::from_iso_string(info.origintimes(boost::numeric_cast<int>(j)));
        }

        theUpdate.producers[info.prodname()] = theseTimes;
      }

      for (int i = 0; i < incomingMessage.handlers_size(); ++i)
      {
        theUpdate.handlers.push_back(incomingMessage.handlers(i));
      }

      itsPendingUpdates.push_back(theUpdate);

      //		  ::dumpMessage(incomingMessage);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ProducerMap SyncGroup::getConsensus() const
{
  return itsConsensus;
}

void SyncGroup::setBaseline(const ProducerMap& theUpdate)
{
  // Clear the consensus and set the new baseline consensus. Updates are set_differenced to this
  itsConsensus = theUpdate;
}

SyncGroup::SyncGroup() {}

SyncGroup::SyncGroup(const ProducerMap& theMap) : itsConsensus(theMap) {}

void SyncGroup::update(const ProducerMap& theUpdate)
{
  try
  {
    for (auto it = theUpdate.begin(); it != theUpdate.end(); ++it)
    {
      auto& producerName = it->first;

      auto myProducerIt = itsConsensus.find(producerName);

      // Proceed if both I and the other have the same producer
      if (myProducerIt != itsConsensus.end())
      {
        // Make set intersection between the current contents and the new
        std::size_t result_length = std::max(it->second.size(), myProducerIt->second.size());
        std::vector<bp::ptime> result(result_length);
        auto end = std::set_intersection(it->second.begin(),
                                         it->second.end(),
                                         myProducerIt->second.begin(),
                                         myProducerIt->second.end(),
                                         result.begin());
        result.resize(boost::numeric_cast<std::size_t>(end - result.begin()));
        itsConsensus[producerName].swap(result);
        //			  ::printList(result);
      }
    }

    // Remove the producers which are not in the current update map (they can't be part of the
    // consensus)
    for (auto it = itsConsensus.begin(); it != itsConsensus.end();)
    {
      auto& producerName = it->first;
      auto foreignProducerIt = theUpdate.find(producerName);
      if (foreignProducerIt == theUpdate.end())
      {
        // This data is not in the update, and can't be part of the consensus
        itsConsensus.erase(it);
      }
      ++it;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

SynchronizerConfig::~SynchronizerConfig() {}

SynchronizerConfig::SynchronizerConfig(const std::string& configFile) : ConfigBase(configFile) {}

bool SynchronizerConfig::parse()
{
  try
  {
    try
    {
      itsPort = boost::numeric_cast<unsigned short>(
          get_mandatory_config_param<unsigned int>("synchro.port"));
    }
    catch (...)
    {
      Spine::Exception exception(BCP, "Operation failed!", nullptr);
      itsFailedReason = exception.what();
      return false;
    }

    // If host name is not given, make random string to replace
    itsHostName =
        get_optional_config_param<std::string>("synchro.hostname", ::makeRandomString(10));

    return true;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned short SynchronizerConfig::getPort() const
{
  return itsPort;
}

std::string SynchronizerConfig::getHostName() const
{
  return itsHostName;
}

std::string SynchronizerConfig::getFailedReason() const
{
  return itsFailedReason;
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
