// ======================================================================
/*!
 * \brief Interface of class QEngine
 */
// ======================================================================

#pragma once

#include "Producer.h"
#include "Repository.h"
#include "Synchro.h"
#include <boost/atomic.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <ogr_spatialref.h>
#include <macgyver/Cache.h>
#include <spine/SmartMetEngine.h>
#include <future>
#include <string>
#include <system_error>

class NFmiPoint;

typedef NFmiDataMatrix<NFmiPoint> Coordinates;
typedef std::shared_ptr<Coordinates> CoordinatesPtr;

typedef NFmiDataMatrix<float> Values;
typedef std::shared_ptr<Values> ValuesPtr;

namespace SmartMet
{
namespace Spine
{
class Reactor;
}

namespace Engine
{
namespace Querydata
{
struct RepoManager;

struct CacheReportingStruct
{
  std::size_t coordinate_cache_max_size;
  std::size_t coordinate_cache_size;
  std::size_t values_cache_max_size;
  std::size_t values_cache_size;
};

class Engine : public Spine::SmartMetEngine
{
 private:
  friend class Synchronizer;

  Engine();
  boost::shared_ptr<RepoManager> itsRepoManager;

  boost::shared_ptr<Synchronizer> itsSynchro;

  // get QEngine synchronization information
  Repository::MetaObject getSynchroInfos() const;

  const std::string itsConfigFile;

  // Cached querydata coordinates.
  typedef Fmi::Cache::Cache<std::size_t, std::shared_future<CoordinatesPtr>> CoordinateCache;
  mutable CoordinateCache itsCoordinateCache;

  // Cached querydata values
  typedef Fmi::Cache::Cache<std::size_t, std::shared_future<ValuesPtr>> ValuesCache;
  mutable ValuesCache itsValuesCache;

  int itsActiveThreadCount;

 public:
  // constructor is available only with a libconfig configuration file
  // will also start a background thread to monitor querydata directories
  Engine(const std::string& configfile);

  // request available information
  const ProducerList& producers() const;                    // available producers
  OriginTimes origintimes(const Producer& producer) const;  // available origintimes

  bool hasProducer(const Producer& producer) const;

  CacheReportingStruct getCacheSizes() const;

  // select producer which has relevant data for the coordinate
  Producer find(double longitude,
                double latitude,
                double maxdistance = 60,
                bool usedatamaxdistance = true,
                const std::string& leveltype = "") const;

  Producer find(const ProducerList& producerlist,
                double longitude,
                double latitude,
                double maxdistance = 60,
                bool usedatamaxdistance = true,
                const std::string& leveltype = "") const;

  // data accessors: latest data or specific origintime
  Q get(const Producer& producer) const;

  Q get(const Producer& producer, const OriginTime& origintime) const;

  // get current engine contents
  Repository::ContentTable getEngineContents(const std::string& timeFormat,
                                             const std::string& projectionFormat) const;

  // Get producer data period

  boost::posix_time::time_period getProducerTimePeriod(const Producer& producer) const;

  // Get engine metadata
  std::list<MetaData> getEngineMetadata() const;

  // Get engine metadata with options
  std::list<MetaData> getEngineMetadata(const MetaQueryOptions& theOptions) const;

  // Get synchronized engine metadata
  std::list<MetaData> getEngineSyncMetadata(const std::string& syncGroup) const;

  // Get synchronized engine metadata with options
  std::list<MetaData> getEngineSyncMetadata(const std::string& syncGroup,
                                            const MetaQueryOptions& theOptions) const;
  // Get synchronized producers for given synchronization group
  boost::optional<ProducerMap> getSyncProducers(const std::string& syncGroup) const;

  // Start synchronization with other QEngines
  void startSynchronize(Spine::Reactor* theReactor);

  // get producer's configuration
  const ProducerConfig& getProducerConfig(const std::string& producer) const;

  CoordinatesPtr getWorldCoordinates(const Q& theQ, OGRSpatialReference* theSR) const;

  ValuesPtr getValues(const Q& theQ,
                      std::size_t theValuesHash,
                      boost::posix_time::ptime theTime) const;

  ValuesPtr getValues(const Q& theQ,
                      const Spine::Parameter& theParam,
                      std::size_t theValuesHash,
                      boost::posix_time::ptime theTime) const;

 protected:
  void init();
  void shutdown();
  void shutdownRequestFlagSet();
  std::time_t getConfigModTime();
  boost::atomic<int> lastConfigErrno;
  int getLastConfigErrno();

 private:
  boost::thread configFileWatcher;  // A thread watching for config file changes
  void configFileWatch();           // A function in separate thread checking the config file

};  // class Engine

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
