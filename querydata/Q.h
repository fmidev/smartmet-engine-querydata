// ======================================================================
/*!
 * \brief A proxy NFmiFastQueryInfo iterator to QEngine model data
 *
 * Engine users are provided access to data only through Q objects,
 * not through public NFmiQueryInfo, NFmiFastQueryInfo or NFmiQueryData
 * instances. This ensures memory management is handled correctly
 * and that clients cannot mess things up by doing something they're
 * not supposed to.
 */
// ======================================================================

#pragma once

#include "MetaData.h"
#include "Model.h"
#include "ValidTimeList.h"
#include "ParameterOptions.h"

#include <spine/Thread.h>
#include <spine/TimeSeries.h>

#include <newbase/NFmiParameterName.h>

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#include <list>

class NFmiArea;
class NFmiMetTime;
class NFmiPoint;
class NFmiFastQueryInfo;

class OGRSpatialReference;

namespace Fmi
{
class DEM;
class LandCover;
}

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class QImpl : private boost::noncopyable, public boost::enable_shared_from_this<QImpl>
{
 public:
  QImpl(SharedModel theModel);
  QImpl(const std::vector<SharedModel>& theModels);

  ~QImpl();

  // Avoid using this as much as possible
  boost::shared_ptr<NFmiFastQueryInfo> info();

  boost::shared_ptr<ValidTimeList> validTimes() const;

  const std::string& levelName() const;
  FmiLevelType levelType() const;
  bool isClimatology() const;
  bool isFullGrid() const;

  friend std::size_t hash_value(const QImpl& theQ);
  std::size_t gridHashValue() const;

  NFmiPoint validPoint(const NFmiPoint& theLatLon, double theMaxDist) const;

  // API correspondence with NFmiFastQueryInfo

  const NFmiMetTime& originTime() const;
  boost::posix_time::ptime modificationTime() const;
  double infoVersion() const;

  void resetTime();
  bool firstTime();
  bool lastTime();
  bool nextTime();
  bool previousTime();
  bool isTimeUsable() const;
  bool time(const NFmiMetTime& theTime);
  const NFmiMetTime& validTime() const;

  bool param(FmiParameterName theParam);
  void resetParam();
  bool nextParam(bool ignoreSubParams = true);

  void resetLevel();
  bool firstLevel();
  bool nextLevel();
  float levelValue() const;

  FmiParameterName parameterName() const;  // Param().GetParamIdent()

  bool isGrid() const;
  bool isArea() const;
  const NFmiDataIdent& param() const;
  const NFmiArea& area() const;
  const NFmiGrid& grid() const;
  const NFmiLevel& level() const;

  bool isInside(double theLon, double theLat, double theMaxDist);  // in km

  MetaData metaData();

  float interpolate(const NFmiPoint& theLatLon,
                    const NFmiMetTime& theTime,
                    int theMaxMinuteRange = 0);

  NFmiPoint latLon(long theIndex) const;

  bool isSubParamUsed() const;
  void setIsSubParamUsed(bool theState);

  unsigned long paramIndex(void) const;
  bool paramIndex(unsigned long theIndex);
  unsigned long levelIndex(void) const;
  bool levelIndex(unsigned long theIndex);
  unsigned long timeIndex(void) const;
  bool timeIndex(unsigned long theIndex);
  unsigned long locationIndex(void) const;
  bool locationIndex(unsigned long theIndex);

  bool calcTimeCache(NFmiQueryInfo& theTargetInfo, checkedVector<NFmiTimeCache>& theTimeCache);
  NFmiTimeCache calcTimeCache(const NFmiMetTime& theTime);
  float cachedInterpolation(const NFmiTimeCache& theTimeCache);
  float cachedInterpolation(const NFmiLocationCache& theLocationCache);
  float cachedInterpolation(const NFmiLocationCache& theLocationCache,
                            const NFmiTimeCache& theTimeCache);
  void landscapeCachedInterpolation(NFmiDataMatrix<float>& theMatrix,
                                    const NFmiDataMatrix<NFmiLocationCache>& theLocationCache,
                                    const NFmiTimeCache& theTimeCache,  // Unset value (NoValue())
                                                                        // for native (currently
                                                                        // active) time
                                    const NFmiDataMatrix<float>& theDEMValues,
                                    const NFmiDataMatrix<bool>& theWaterFlags);
  bool calcLatlonCachePoints(NFmiQueryInfo& theTargetInfo,
                             NFmiDataMatrix<NFmiLocationCache>& theLocationCache);

  void values(NFmiDataMatrix<float>& theMatrix,
              const NFmiDataMatrix<float>& theDEMValues =
                  NFmiDataMatrix<float>(),  // DEM values for landscaping
              const NFmiDataMatrix<bool>& theWaterFlags =
                  NFmiDataMatrix<bool>()  // Water flags for landscaping
              );
  void values(NFmiDataMatrix<float>& theMatrix,
              const NFmiMetTime& theInterpolatedTime,
              const NFmiDataMatrix<float>& theDEMValues =
                  NFmiDataMatrix<float>(),  // DEM values for landscaping
              const NFmiDataMatrix<bool>& theWaterFlags =
                  NFmiDataMatrix<bool>()  // Water flags for landscaping
              );
  void values(const NFmiDataMatrix<NFmiPoint>& theLatlonMatrix,
              NFmiDataMatrix<float>& theValues,
              const NFmiMetTime& theTime,
              float P = kFloatMissing,
              float H = kFloatMissing);

  void croppedValues(NFmiDataMatrix<float>& theMatrix,
                     int x1,
                     int y1,
                     int x2,
                     int y2,
                     const NFmiDataMatrix<float>& theDEMValues =
                         NFmiDataMatrix<float>(),  // DEM values for landscaping
                     const NFmiDataMatrix<bool>& theWaterFlags =
                         NFmiDataMatrix<bool>()  // Water flags for landscaping
                     ) const;

  void pressureValues(NFmiDataMatrix<float>& theValues,
                      const NFmiMetTime& theInterpolatedTime,
                      float wantedPressureLevel);
  void pressureValues(NFmiDataMatrix<float>& theValues,
                      const NFmiGrid& theWantedGrid,
                      const NFmiMetTime& theInterpolatedTime,
                      float wantedPressureLevel);

  boost::shared_ptr<std::vector<NFmiPoint>> latLonCache() const;

  // Gridded landscaping; Load dem values and water flags for native (sub)grid or for given
  // locations

  bool loadDEMAndWaterFlags(const Fmi::DEM& theDEM,
                            const Fmi::LandCover& theLandCover,
                            double theResolution,
                            const NFmiDataMatrix<NFmiLocationCache>& locCache,
                            NFmiDataMatrix<float>& demMatrix,
                            NFmiDataMatrix<bool>& waterFlagMatrix,
                            int x1 = 0,
                            int y1 = 0,
                            int x2 = 0,
                            int y2 = 0) const;

  // sample data into a new projection

  boost::shared_ptr<QImpl> sample(const SmartMet::Spine::Parameter& theParameter,
                                  const boost::posix_time::ptime& theTime,
                                  const OGRSpatialReference& theCrs,
                                  double theXmin,
                                  double theYmin,
                                  double theXmax,
                                  double theYmax,
                                  double theResolution,
                                  const Fmi::DEM& theDem,
                                  const Fmi::LandCover& theLandCover);

  // one location, one timestep
  SmartMet::Spine::TimeSeries::Value value(ParameterOptions& param,
                                           const boost::local_time::local_date_time& ldt);
  // one location, many timesteps
  SmartMet::Spine::TimeSeries::TimeSeriesPtr values(
      ParameterOptions& param, const SmartMet::Spine::TimeSeriesGenerator::LocalTimeList& tlist);
  // many locations (indexmask), many timesteps
  SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr values(
      ParameterOptions& param,
      const NFmiIndexMask& indexmask,
      const SmartMet::Spine::TimeSeriesGenerator::LocalTimeList& tlist);
  // many locations (llist), many timesteps
  SmartMet::Spine::TimeSeries::TimeSeriesGroupPtr values(
      ParameterOptions& param,
      const SmartMet::Spine::LocationList& llist,
      const SmartMet::Spine::TimeSeriesGenerator::LocalTimeList& tlist,
      const double& maxdistance);

  bool selectLevel(double theLevel);

  const WGS84Envelope& getWGS84Envelope();

 private:
  QImpl();

  std::vector<SharedModel> itsModels;
  std::vector<SharedInfo> itsInfos;  // used only in destructor and MultiInfo constructor
  boost::shared_ptr<NFmiFastQueryInfo> itsInfo;    // or NFmiMultiQueryInfo
  boost::shared_ptr<ValidTimeList> itsValidTimes;  // collective over all datas
  std::size_t itsHashValue;

  Spine::MutexType itsWGS84EnvelopeMutex;
  WGS84Envelope::Unique itsWGS84Envelope;

};  // class QImpl

typedef boost::shared_ptr<QImpl> Q;
typedef std::list<Q> QList;

inline std::size_t hash_value(const Q& theQ)
{
  return hash_value(*theQ);
}

}  // namespace Q
}  // namespace Engine
}  // namespace SmartMet
