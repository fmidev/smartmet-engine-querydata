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

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <newbase/NFmiFastQueryInfo.h>
#include <spine/Thread.h>
#include <list>

class NFmiPoint;
class NFmiQueryData;

typedef boost::shared_ptr<NFmiFastQueryInfo> SharedInfo;

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class ValidPoints;

class Model : private boost::noncopyable, public boost::enable_shared_from_this<Model>
{
 public:
  Model(const boost::filesystem::path& filename,
        const std::string& validpointscachedir,
        Producer producer,
        std::string levelname,
        bool climatology,
        bool full,
        bool relativeuv,
        unsigned int update_interval,
        unsigned int minimum_expiration_time,
        bool mmap);

  Model(const Model& theModel, boost::shared_ptr<NFmiQueryData> theData, std::size_t theHash);

  Model(boost::shared_ptr<NFmiQueryData> theData, std::size_t theHash);

  Model() = delete;
  Model(const Model& theModel) = delete;
  Model& operator=(const Model& theModel) = delete;

  friend std::size_t hash_value(const Model& theModel);

  const boost::posix_time::ptime& originTime() const;
  const boost::posix_time::ptime& loadTime() const;
  const boost::posix_time::ptime& modificationTime() const;
  boost::posix_time::ptime expirationTime() const;

  boost::shared_ptr<ValidTimeList> validTimes() const;

  const boost::filesystem::path& path() const;
  const Producer& producer() const;

  const std::string& levelName() const;
  bool isClimatology() const;
  bool isFullGrid() const;
  bool isRelativeUV() const;

  NFmiPoint validPoint(const NFmiPoint& theLatLon, double theMaxDist) const;

  std::size_t gridHashValue() const;
#ifndef WGS84
  void setLatLonCache(boost::shared_ptr<std::vector<NFmiPoint>> theCache);
  boost::shared_ptr<std::vector<NFmiPoint>> makeLatLonCache();
#endif

  void uncache() const;

 private:
  // These need to be able to return the info object back:
  friend class QImpl;
  friend class Repository;
  SharedInfo info() const;
  void release(boost::shared_ptr<NFmiFastQueryInfo> theInfo) const;

  std::size_t itsHashValue;
  boost::posix_time::ptime itsOriginTime;
  boost::posix_time::ptime itsLoadTime;
  boost::filesystem::path itsPath;
  boost::posix_time::ptime itsModificationTime;
  Producer itsProducer;
  std::string itsLevelName;
  unsigned int itsUpdateInterval;
  unsigned int itsMinimumExpirationTime;
  bool itsClimatology = false;
  bool itsFullGrid = false;
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

typedef boost::shared_ptr<Model> SharedModel;
typedef std::list<SharedModel> SharedModelList;
typedef std::list<std::pair<SharedModel, ValidTimeList>> SharedModelTimeList;

inline std::size_t hash_value(const SharedModel& theModel)
{
  return hash_value(*theModel);
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
