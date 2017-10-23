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
class Repository
{
 public:
  Repository();

  void add(const ProducerConfig& config);
  void add(const Producer& producer, SharedModel model);

  void remove(const Producer& producer, const boost::filesystem::path& path);
  void resize(const Producer& producer, std::size_t limit);

  Producer find(const ProducerList& producerlist,
                const ProducerList& producerorder,
                double lon,
                double lat,
                double maxdist,
                bool usedatamaxdist,
                const std::string& leveltype) const;

  OriginTimes originTimes(const Producer& producer) const;

  // Must not use aliases for these!
  Q get(const Producer& producer) const;
  Q get(const Producer& producer, const OriginTime& origintime) const;

  Q getAll(const Producer& producer) const;

  typedef std::pair<boost::shared_ptr<Spine::Table>, Spine::TableFormatter::Names> ContentTable;
  typedef std::map<OriginTime, SharedModel> SharedModels;
  typedef std::map<std::string, std::vector<boost::posix_time::ptime> > MetaObject;

  ContentTable getRepoContents(const std::string& timeFormat,
                               const std::string& projectionFormat) const;

  std::list<MetaData> getRepoMetadata() const;

  std::list<MetaData> getRepoMetadata(const MetaQueryOptions& options) const;

  MetaObject getSynchroInfos() const;

  SharedModel getModel(const Producer& producer, const boost::filesystem::path& path) const;
  SharedModels getAllModels(const Producer& producer) const;

 private:
  bool contains(const SharedModels& models,
                double lon,
                double lat,
                double maxdist,
                const std::string& levelname) const;

  // Each uniquely named producer has a number
  // of models, which are sorted by their origin times

  typedef std::map<Producer, SharedModels> Producers;
  typedef std::map<Producer, ProducerConfig> ProducerConfigs;
  Producers itsProducers;
  ProducerConfigs itsProducerConfigs;

};  // class Repository

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
