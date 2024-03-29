// ======================================================================
/*!
 * \brief A single model
 *
 * A model is not intended to be copied, its life is managed by the
 * RepoManager. Only shared copies are given to users so that the
 * repo may delete the model even though some parts of it may still
 * be in use.
 */
// ======================================================================

#pragma once

#include "Producer.h"
#include "ValidTimeList.h"
#include <macgyver/DateTime.h>
#include <boost/filesystem/path.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <newbase/NFmiFastQueryInfo.h>
#include <spine/Thread.h>
#include <list>

class NFmiPoint;
class NFmiQueryData;

using SharedInfo = boost::shared_ptr<NFmiFastQueryInfo>;

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class ValidPoints;

class Model : public boost::enable_shared_from_this<Model>
{
 public:
  Model(const boost::filesystem::path& filename,
        const std::string& validpointscachedir,
        Producer producer,
        std::string levelname,
        bool climatology,
        bool full,
        bool staticgrid,
        bool relativeuv,
        unsigned int update_interval,
        unsigned int minimum_expiration_time,
        bool mmap);

  Model(const Model& theModel, boost::shared_ptr<NFmiQueryData> theData, std::size_t theHash);

  Model(boost::shared_ptr<NFmiQueryData> theData, std::size_t theHash);

  ~Model() = default;
  Model() = delete;
  Model(const Model& theModel) = delete;
  Model& operator=(const Model& theModel) = delete;
  Model(Model&& theModel) = delete;
  Model& operator=(Model&& theModel) = delete;

  friend std::size_t hash_value(const Model& theModel);

  const Fmi::DateTime& originTime() const;
  const Fmi::DateTime& loadTime() const;
  const Fmi::DateTime& modificationTime() const;
  Fmi::DateTime expirationTime() const;

  boost::shared_ptr<ValidTimeList> validTimes() const;

  const boost::filesystem::path& path() const;
  const Producer& producer() const;

  const std::string& levelName() const;
  bool isClimatology() const;
  bool isFullGrid() const;
  bool isStaticGrid() const;
  bool isRelativeUV() const;

  NFmiPoint validPoint(const NFmiPoint& theLatLon, double theMaxDist) const;

  std::size_t gridHashValue() const;

  // Deprecated in WGS84 branch
  void setLatLonCache(const boost::shared_ptr<std::vector<NFmiPoint>>& theCache);
  boost::shared_ptr<std::vector<NFmiPoint>> makeLatLonCache();

  void uncache() const;

 private:
  // These need to be able to return the info object back:
  friend class QImpl;
  friend class Repository;
  friend struct RepoManager;
  SharedInfo info() const;
  void release(const boost::shared_ptr<NFmiFastQueryInfo>& theInfo) const;

  std::size_t itsHashValue = 0;
  Fmi::DateTime itsOriginTime;
  Fmi::DateTime itsLoadTime;
  boost::filesystem::path itsPath;
  Fmi::DateTime itsModificationTime;
  Producer itsProducer;
  std::string itsLevelName;
  unsigned int itsUpdateInterval = 0;
  unsigned int itsMinimumExpirationTime = 999999;
  bool itsClimatology = false;
  bool itsFullGrid = true;
  bool itsStaticGrid = false;
  bool itsRelativeUV = false;

  boost::shared_ptr<ValidPoints> itsValidPoints;
  boost::shared_ptr<ValidTimeList> itsValidTimeList;

  // Constructing NFmiFastQueryInfo may be slow if there are many
  // time steps or many locations - hence we pool the used infos.
  // The info is returned via a proxy which returns the info back
  // to the pool.

  mutable Spine::MutexType itsQueryInfoPoolMutex;
  mutable std::list<SharedInfo> itsQueryInfoPool;

  // The actual reference to the data is after the pool above to make
  // sure the destruction order makes sense.
  boost::shared_ptr<NFmiQueryData> itsQueryData;
};

using SharedModel = boost::shared_ptr<Model>;
using SharedModelList = std::list<SharedModel>;
using SharedModelTimeList = std::list<std::pair<SharedModel, ValidTimeList>>;

inline std::size_t hash_value(const SharedModel& theModel)
{
  return hash_value(*theModel);
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
