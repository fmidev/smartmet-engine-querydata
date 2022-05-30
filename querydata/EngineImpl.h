// ======================================================================
/*!
 * \brief Interface of class QEngine
 */
// ======================================================================

#pragma once

#include "Engine.h"
#include "ParameterTranslations.h"
#include "Producer.h"
#include "Repository.h"
#include "Synchro.h"
#include <boost/atomic.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <gis/CoordinateMatrix.h>
#include <macgyver/Cache.h>
#include <spine/SmartMetEngine.h>
#include <future>
#include <string>
#include <system_error>

class NFmiPoint;
class OGRSpatialReference;

using CoordinatesPtr = std::shared_ptr<Fmi::CoordinateMatrix>;

using Values = NFmiDataMatrix<float>;
using ValuesPtr = std::shared_ptr<Values>;

namespace Fmi
{
class SpatialReference;
}

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

class EngineImpl final : public Engine
{
 private:
  friend class Synchronizer;

  boost::atomic_shared_ptr<RepoManager> itsRepoManager;

  boost::shared_ptr<Synchronizer> itsSynchro;

  // get QEngine synchronization information
  Repository::MetaObject getSynchroInfos() const override;

  const std::string itsConfigFile;

  // Cached querydata coordinates.
  using CoordinateCache = Fmi::Cache::Cache<std::size_t, std::shared_future<CoordinatesPtr>>;
  mutable CoordinateCache itsCoordinateCache;

  // Cached querydata values
  using ValuesCache = Fmi::Cache::Cache<std::size_t, std::shared_future<ValuesPtr>>;
  mutable ValuesCache itsValuesCache;

  int itsActiveThreadCount;

  boost::atomic_shared_ptr<ParameterTranslations> itsParameterTranslations;

 protected:
  // constructor is available only with a libconfig configuration file
  // will also start a background thread to monitor querydata directories
  EngineImpl(const std::string& configfile);

public:
  EngineImpl() = delete;

  // Factory method
  static Engine* create(const std::string& configfile);

  // request available information
  const ProducerList& producers() const override;                    // available producers
  OriginTimes origintimes(const Producer& producer) const override;  // available origintimes

  bool hasProducer(const Producer& producer) const override;

  CacheReportingStruct getCacheSizes() const override;

  // select producer which has relevant data for the coordinate
  Producer find(double longitude,
                double latitude,
                double maxdistance = 60,
                bool usedatamaxdistance = true,
                const std::string& leveltype = "") const override;

  Producer find(const ProducerList& producerlist,
                double longitude,
                double latitude,
                double maxdistance = 60,
                bool usedatamaxdistance = true,
                const std::string& leveltype = "") const override;

  // data accessors: latest data or specific origintime
  Q get(const Producer& producer) const override;

  Q get(const Producer& producer, const OriginTime& origintime) const override;

  // Get detailed info of current producers
  Repository::ContentTable getProducerInfo(const std::string& timeFormat,
                                           boost::optional<std::string> producer) const override;
  // Get info of parameters of each producer
  Repository::ContentTable getParameterInfo(boost::optional<std::string> producer) const override;

protected:
  // Get current engine contents
  Repository::ContentTable
  getEngineContentsForAllProducers(const std::string& timeFormat,
                                   const std::string& projectionFormat) const override;
  Repository::ContentTable
  getEngineContentsForProducer(const std::string& producer,
                               const std::string& timeFormat,
                               const std::string& projectionFormat) const override;

public:
  // Get producer data period

  boost::posix_time::time_period getProducerTimePeriod(const Producer& producer) const override;

protected:
  // Get engine metadata
  std::list<MetaData> getEngineMetadataBasic() const override;

  // Get engine metadata with options
  std::list<MetaData> getEngineMetadataWithOptions(const MetaQueryOptions& theOptions) const override;

  // Get synchronized engine metadata
    std::list<MetaData> getEngineSyncMetadataBasic(const std::string& syncGroup) const override;

  // Get synchronized engine metadata with options
  std::list<MetaData>
  getEngineSyncMetadataWithOptions(const std::string& syncGroup,
                                   const MetaQueryOptions& theOptions) const override;

public:
  // Get synchronized producers for given synchronization group
    boost::optional<ProducerMap> getSyncProducers(const std::string& syncGroup) const override;

  // Start synchronization with other QEngines
    void startSynchronize(Spine::Reactor* theReactor) override;

  // get producer's configuration
  const ProducerConfig& getProducerConfig(const std::string& producer) const override;

protected:
  CoordinatesPtr getWorldCoordinatesDefault(const Q& theQ) const override;

  CoordinatesPtr getWorldCoordinatesForSR(const Q& theQ, const Fmi::SpatialReference& theSR) const override;

  ValuesPtr getValuesDefault(const Q& theQ,
                             std::size_t theValuesHash,
                             boost::posix_time::ptime theTime) const override;

  ValuesPtr getValuesForParam(const Q& theQ,
                              const Spine::Parameter& theParam,
                              std::size_t theValuesHash,
                              boost::posix_time::ptime theTime) const override;

 protected:
  void init() override;
  void shutdown() override;
  std::time_t getConfigModTime();
  boost::atomic<int> lastConfigErrno;
  int getLastConfigErrno();

 private:
  boost::thread configFileWatcher;  // A thread watching for config file changes
  void configFileWatch();           // A function in separate thread checking the config file
  Fmi::Cache::CacheStatistics getCacheStats() const override;  // Get cache statistics

};  // class EngineImpl

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
