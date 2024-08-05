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
#include <filesystem>
#include <memory>
#include <newbase/NFmiFastQueryInfo.h>
#include <spine/Thread.h>
#include <list>

class NFmiPoint;
class NFmiQueryData;

using SharedInfo = std::shared_ptr<NFmiFastQueryInfo>;

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class ValidPoints;

class Model : public boost::enable_shared_from_this<Model>
{
  struct Private { explicit Private() = default; }; // Dummy structure to disable public constructors
 public:
  Model(Private,
        const std::filesystem::path& filename,
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

  Model(Private,
        const Model& theModel,
        std::shared_ptr<NFmiQueryData> theData,
        std::size_t theHash);

  Model(Private, std::shared_ptr<NFmiQueryData> theData, std::size_t theHash);

  static std::shared_ptr<Model> create(
        const std::filesystem::path& filename,
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

  static std::shared_ptr<Model> create(
        const Model& theModel,
        std::shared_ptr<NFmiQueryData> theData,
        std::size_t theHash);

  static std::shared_ptr<Model> create(
        std::shared_ptr<NFmiQueryData> theData,
        std::size_t theHash);

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

  std::shared_ptr<ValidTimeList> validTimes() const;

  const std::filesystem::path& path() const;
  const Producer& producer() const;

  const std::string& levelName() const;
  bool isClimatology() const;
  bool isFullGrid() const;
  bool isStaticGrid() const;
  bool isRelativeUV() const;

  NFmiPoint validPoint(const NFmiPoint& theLatLon, double theMaxDist) const;

  std::size_t gridHashValue() const;

  // Deprecated in WGS84 branch
  void setLatLonCache(const std::shared_ptr<std::vector<NFmiPoint>>& theCache);
  std::shared_ptr<std::vector<NFmiPoint>> makeLatLonCache();

  void uncache() const;

 private:
  // These need to be able to return the info object back:
  friend class QImpl;
  friend class Repository;
  friend struct RepoManager;
  SharedInfo info() const;
  void release(const std::shared_ptr<NFmiFastQueryInfo>& theInfo) const;

  std::size_t itsHashValue = 0;
  Fmi::DateTime itsOriginTime;
  Fmi::DateTime itsLoadTime;
  std::filesystem::path itsPath;
  Fmi::DateTime itsModificationTime;
  Producer itsProducer;
  std::string itsLevelName;
  unsigned int itsUpdateInterval = 0;
  unsigned int itsMinimumExpirationTime = 999999;
  bool itsClimatology = false;
  bool itsFullGrid = true;
  bool itsStaticGrid = false;
  bool itsRelativeUV = false;

  std::shared_ptr<ValidPoints> itsValidPoints;
  std::shared_ptr<ValidTimeList> itsValidTimeList;

  // Constructing NFmiFastQueryInfo may be slow if there are many
  // time steps or many locations - hence we pool the used infos.
  // The info is returned via a proxy which returns the info back
  // to the pool.

  mutable Spine::MutexType itsQueryInfoPoolMutex;
  mutable std::list<SharedInfo> itsQueryInfoPool;

  // The actual reference to the data is after the pool above to make
  // sure the destruction order makes sense.
  std::shared_ptr<NFmiQueryData> itsQueryData;
};

using SharedModel = std::shared_ptr<Model>;
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
