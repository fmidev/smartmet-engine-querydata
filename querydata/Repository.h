// ======================================================================
/*!
 * \brief Model repository
 */
// ======================================================================

#pragma once

#include "MetaData.h"
#include "MetaQueryOptions.h"
#include "Model.h"
#include "OriginTime.h"
#include "Producer.h"
#include "Q.h"

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/shared_ptr.hpp>
#include <set>
#include <string>
#include <utility>

#include <macgyver/TimeFormatter.h>
#include <spine/Table.h>
#include <spine/TableFormatter.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
struct ProducerStatus
{
  boost::posix_time::ptime latest_scan_time{boost::posix_time::not_a_date_time};
  boost::posix_time::ptime next_scan_time{boost::posix_time::not_a_date_time};
  boost::posix_time::ptime latest_data_load_time{boost::posix_time::not_a_date_time};
  unsigned int number_of_loaded_files{0};
};

class Repository
{
 public:
  Repository() = default;

  void add(const ProducerConfig& config);
  void add(const Producer& producer, SharedModel model);

  void remove(const Producer& producer, const boost::filesystem::path& path);
  void resize(const Producer& producer, std::size_t limit);
  void expire(const Producer& producer, std::size_t max_age);

  Producer find(const ProducerList& producerlist,
                const ProducerList& producerorder,
                double lon,
                double lat,
                double maxdist,
                bool usedatamaxdist,
                const std::string& leveltype,
                bool checkLatestModelAge = false) const;

  OriginTimes originTimes(const Producer& producer) const;

  bool hasProducer(const Producer& producer) const;

  // Must not use aliases for these!
  Q get(const Producer& producer) const;
  Q get(const Producer& producer, const OriginTime& origintime) const;
  Q get(const Producer& producer, const boost::posix_time::time_period& timeperiod) const;

  Q getAll(const Producer& producer) const;

  using ContentTable = std::pair<boost::shared_ptr<Spine::Table>, Spine::TableFormatter::Names>;
  using SharedModels = std::map<OriginTime, SharedModel>;
  using MetaObject = std::map<std::string, std::vector<boost::posix_time::ptime> >;

  ContentTable getProducerInfo(const ProducerList& producerlist,
                               const std::string& timeFormat) const;
  ContentTable getParameterInfo(const ProducerList& producerlist) const;
  ContentTable getRepoContents(const std::string& timeFormat,
                               const std::string& projectionFormat) const;
  ContentTable getRepoContents(const std::string& producer,
                               const std::string& timeFormat,
                               const std::string& projectionFormat) const;

  // All metadata
  std::list<MetaData> getRepoMetadata() const;
  // Filter all metadata based on options
  std::list<MetaData> getRepoMetadata(const MetaQueryOptions& options) const;
  // Metadata for a specific producer
  std::list<MetaData> getRepoMetadata(const std::string& producer) const;
  // Metadata for a specific producer and origintime
  std::list<MetaData> getRepoMetadata(const std::string& producer,
                                      const boost::posix_time::ptime& origintime) const;

  MetaObject getSynchroInfos() const;

  SharedModel getModel(const Producer& producer, const boost::filesystem::path& path) const;
  SharedModels getAllModels(const Producer& producer) const;

  void updateProducerStatus(const std::string& producer,
                            const boost::posix_time::ptime& scanTime,
                            const boost::posix_time::ptime& nextScanTime);
  void updateProducerStatus(const std::string& producer,
                            const boost::posix_time::ptime& dataLoadTime,
                            unsigned int nFiles);

  void verbose(bool flag);

 private:
  bool contains(const SharedModels& models,
                double lon,
                double lat,
                double maxdist,
                const std::string& levelname) const;

  // Each uniquely named producer has a number
  // of models, which are sorted by their origin times

  using Producers = std::map<Producer, SharedModels>;
  using ProducerConfigs = std::map<Producer, ProducerConfig>;
  Producers itsProducers;
  ProducerConfigs itsProducerConfigs;
  bool itsVerbose = false;
  std::map<std::string, ProducerStatus> itsProducerStatus;

  const SharedModels& findProducer(const std::string& producer) const;

};  // class Repository

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
