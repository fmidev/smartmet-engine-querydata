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

#include <macgyver/DateTime.h>
#include <memory>
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
  Fmi::DateTime latest_scan_time{Fmi::DateTime::NOT_A_DATE_TIME};
  Fmi::DateTime next_scan_time{Fmi::DateTime::NOT_A_DATE_TIME};
  Fmi::DateTime latest_data_load_time{Fmi::DateTime::NOT_A_DATE_TIME};
  unsigned int number_of_loaded_files{0};
};

class Repository
{
 public:
  Repository() = default;

  void add(const ProducerConfig& config);
  void add(const Producer& producer, const SharedModel& model);

  void remove(const Producer& producer, const std::filesystem::path& path);
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

  // The models a Q would be constructed from. Public only because the helpers
  // turning a selection into a Q or into a hash value live in Repository.cpp.
  struct Selection
  {
    std::vector<SharedModel> models;
    // True if QImpl would be constructed from a single model instead of a view
    // over several ones. The two constructors calculate the hash value in
    // different ways, hence the distinction matters.
    bool single = false;
  };

  Selection select(const Producer& producer) const;
  Selection select(const Producer& producer, const OriginTime& origintime) const;
  Selection select(const Producer& producer, const Fmi::TimePeriod& timeperiod) const;
  Selection selectAll(const Producer& producer) const;

  // Must not use aliases for these!
  Q get(const Producer& producer) const;
  Q get(const Producer& producer, const OriginTime& origintime) const;
  Q get(const Producer& producer, const Fmi::TimePeriod& timeperiod) const;

  Q getAll(const Producer& producer) const;

  // Hash value and expiration time of the data get() would return, without
  // constructing a Q. Used for ETag calculation.
  ModelHashValue getModelHashValue(const Producer& producer) const;
  ModelHashValue getModelHashValue(const Producer& producer, const OriginTime& origintime) const;
  ModelHashValue getModelHashValue(const Producer& producer,
                                   const Fmi::TimePeriod& timeperiod) const;

  using ContentTable = std::unique_ptr<Spine::Table>;
  using SharedModels = std::map<OriginTime, SharedModel>;
  using MetaObject = std::map<std::string, std::vector<Fmi::DateTime> >;

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
                                      const Fmi::DateTime& origintime) const;

  MetaObject getSynchroInfos() const;

  SharedModel getModel(const Producer& producer, const std::filesystem::path& path) const;
  SharedModels getAllModels(const Producer& producer) const;

  void updateProducerStatus(const std::string& producer,
                            const Fmi::DateTime& scanTime,
                            const Fmi::DateTime& nextScanTime);
  void updateProducerStatus(const std::string& producer,
                            const Fmi::DateTime& dataLoadTime,
                            unsigned int nFiles);

  void verbose(bool flag);

 private:
  // Member instead of anonymous in cpp since we need "friend" access rights
  static bool contains(const SharedModels& models,
                       double lon,
                       double lat,
                       double maxdist,
                       const std::string& levelname);

  // Each uniquely named producer has a number of models, which are sorted by their origin times

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
