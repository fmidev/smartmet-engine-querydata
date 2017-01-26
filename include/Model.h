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

#include <spine/Thread.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <boost/filesystem/path.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
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
        const Producer& producer,
        const std::string& levelname,
        bool climatology,
        bool full);

  Model(const Model& theModel, boost::shared_ptr<NFmiQueryData> theData, std::size_t theHash);

  friend std::size_t hash_value(const Model& theModel);

  const boost::posix_time::ptime& originTime() const;
  const boost::posix_time::ptime& modificationTime() const;

  boost::shared_ptr<ValidTimeList> validTimes() const;

  const boost::filesystem::path& path() const;
  const Producer& producer() const;

  const std::string& levelName() const;
  bool isClimatology() const;
  bool isFullGrid() const;

  NFmiPoint validPoint(const NFmiPoint& theLatLon, double theMaxDist) const;

  std::size_t gridHashValue() const;
  void setLatLonCache(boost::shared_ptr<std::vector<NFmiPoint>> theCache);
  boost::shared_ptr<std::vector<NFmiPoint>> makeLatLonCache();

 private:
  Model();
  Model(const Model& theModel);
  Model& operator=(const Model& theModel);

  // These need to be able to return the info object back:
  friend class QImpl;
  friend class Repository;
  SharedInfo info() const;
  void release(boost::shared_ptr<NFmiFastQueryInfo> theInfo) const;

  std::size_t itsHashValue;
  boost::posix_time::ptime itsOriginTime;
  boost::filesystem::path itsPath;
  boost::posix_time::ptime itsModificationTime;
  Producer itsProducer;
  std::string itsLevelName;
  bool itsClimatology;
  bool itsFullGrid;

  // We need direct access to data in the manager
  boost::shared_ptr<NFmiQueryData> itsQueryData;

  boost::shared_ptr<ValidPoints> itsValidPoints;
  boost::shared_ptr<ValidTimeList> itsValidTimeList;

  // Constructing NFmiFastQueryInfo may be slow if there are many
  // time steps or many locations - hence we pool the used infos.
  // The info is returned via a proxy which returns the info back
  // to the pool.

  mutable SmartMet::Spine::MutexType itsQueryInfoPoolMutex;
  mutable std::list<SharedInfo> itsQueryInfoPool;
};

typedef boost::shared_ptr<Model> SharedModel;
typedef std::list<SharedModel> SharedModelList;
typedef std::list<std::pair<SharedModel, ValidTimeList>> SharedModelTimeList;

inline std::size_t hash_value(const SharedModel& theModel)
{
  return hash_value(*theModel);
}

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
