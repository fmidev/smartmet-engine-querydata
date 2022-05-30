#pragma once

#include "OriginTime.h"
#include "Producer.h"
#include "Repository.h"
#include "RepoManager.h"
#include <gis/CoordinateMatrix.h>
#include <spine/SmartMetEngine.h>

#include "ParameterTranslations.h"
#include "Synchro.h"
#include <macgyver/Cache.h>

using CoordinatesPtr = std::shared_ptr<Fmi::CoordinateMatrix>;

using Values = NFmiDataMatrix<float>;
using ValuesPtr = std::shared_ptr<Values>;

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
struct CacheReportingStruct
{
  std::size_t coordinate_cache_max_size;
  std::size_t coordinate_cache_size;
  std::size_t values_cache_max_size;
  std::size_t values_cache_size;
};

class Engine : public Spine::SmartMetEngine
{
 public:
  Engine();

  virtual ~Engine();

  /**
   *   @brief Return available producers
   */
  virtual const ProducerList& producers() const;

  /**
   *   @brief Return available origintimes
   **/
  virtual OriginTimes origintimes(const Producer& producer) const;

  virtual bool hasProducer(const Producer& producer) const;

  virtual CacheReportingStruct getCacheSizes() const;

  // select producer which has relevant data for the coordinate
  virtual Producer find(double longitude,
                double latitude,
                double maxdistance = 60,
                bool usedatamaxdistance = true,
                const std::string& leveltype = "") const;

  virtual Producer find(const ProducerList& producerlist,
                double longitude,
                double latitude,
                double maxdistance = 60,
                bool usedatamaxdistance = true,
                const std::string& leveltype = "") const;

  /**
   *   @brief data accessor: latest data
   */
  virtual Q get(const Producer& producer) const;

  /**
   *   @brief data accessor: specific origintime
   */
  virtual Q get(const Producer& producer, const OriginTime& origintime) const;


  /**
   *  @brief Get detailed info of current producers
   */
  virtual Repository::ContentTable getProducerInfo(const std::string& timeFormat,
                                                   boost::optional<std::string> producer) const;

  /**
   *  @brief Get info of parameters of each producer
   */
  virtual Repository::ContentTable getParameterInfo(boost::optional<std::string> producer) const;

  /**
   *  @brief Get current engine contents
   */
  inline Repository::ContentTable
  getEngineContents(const std::string& timeFormat,
                    const std::string& projectionFormat) const
  {
    return getEngineContentsForAllProducers(timeFormat, projectionFormat);
  }

  inline Repository::ContentTable
  getEngineContents(const std::string& producer,
                    const std::string& timeFormat,
                    const std::string& projectionFormat) const
  {
    return getEngineContentsForProducer(producer, timeFormat, projectionFormat);
  }

  /**
   *  @brief  Get producer data period
   */
  virtual boost::posix_time::time_period getProducerTimePeriod(const Producer& producer) const;

  /**
   *  @brief Get engine metadata
   */
  inline std::list<MetaData> getEngineMetadata() const
  {
    return getEngineMetadataBasic();
  }

  /**
   *  @brief Get engine metadata with options
   */
  inline std::list<MetaData> getEngineMetadata(const MetaQueryOptions& theOptions) const
  {
    return getEngineMetadataWithOptions(theOptions);
  }

  /**
   *  @brief Get synchronized engine metadata
   */
  inline std::list<MetaData> getEngineSyncMetadata(const std::string& syncGroup) const
  {
    return getEngineSyncMetadataBasic(syncGroup);
  }

  /**
   *  @brief Get synchronized engine metadata with options
   */
  inline std::list<MetaData> getEngineSyncMetadata(const std::string& syncGroup,
                                                   const MetaQueryOptions& theOptions) const
  {
    return getEngineSyncMetadataWithOptions(syncGroup, theOptions);
  }

  virtual Repository::MetaObject getSynchroInfos() const;

  /**
   *  @brief Get synchronized producers for given synchronization group
   */
  virtual boost::optional<ProducerMap> getSyncProducers(const std::string& syncGroup) const;

  /**
   *  @brief Start synchronization with other QEngines
   */
  virtual void startSynchronize(Spine::Reactor* theReactor);

  virtual const ProducerConfig& getProducerConfig(const std::string& producer) const;

  inline CoordinatesPtr getWorldCoordinates(const Q& theQ) const
  {
    return getWorldCoordinatesDefault(theQ);
  }

  inline CoordinatesPtr getWorldCoordinates(const Q& theQ, const Fmi::SpatialReference& theSR) const
  {
    return getWorldCoordinatesForSR(theQ, theSR);
  }

  inline ValuesPtr getValues(const Q& theQ,
                             std::size_t theValuesHash,
                             boost::posix_time::ptime theTime) const
  {
    return getValuesDefault(theQ, theValuesHash, theTime);
  }

  inline ValuesPtr getValues(const Q& theQ,
                             const Spine::Parameter& theParam,
                             std::size_t theValuesHash,
                             boost::posix_time::ptime theTime) const
  {
    return getValuesForParam(theQ, theParam, theValuesHash, theTime);
  }

 protected:
  virtual Repository::ContentTable
    getEngineContentsForAllProducers(const std::string& timeFormat,
                                     const std::string& projectionFormat) const;

  virtual Repository::ContentTable
    getEngineContentsForProducer(const std::string& producer,
                                 const std::string& timeFormat,
                                 const std::string& projectionFormat) const;

  virtual std::list<MetaData> getEngineMetadataBasic() const;

  virtual std::list<MetaData> getEngineMetadataWithOptions(const MetaQueryOptions& theOptions) const;

  virtual std::list<MetaData> getEngineSyncMetadataBasic(const std::string& syncGroup) const;

  virtual std::list<MetaData> getEngineSyncMetadataWithOptions(const std::string& syncGroup,
                                                               const MetaQueryOptions& theOptions) const;


  virtual CoordinatesPtr getWorldCoordinatesDefault(const Q& theQ) const;

  virtual CoordinatesPtr getWorldCoordinatesForSR(const Q& theQ, const Fmi::SpatialReference& theSR) const;

  virtual ValuesPtr getValuesDefault(const Q& theQ,
                                     std::size_t theValuesHash,
                                     boost::posix_time::ptime theTime) const;

  virtual ValuesPtr getValuesForParam(const Q& theQ,
                                      const Spine::Parameter& theParam,
                                      std::size_t theValuesHash,
                                      boost::posix_time::ptime theTime) const;

  void init() override;

  void shutdown() override;

};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
