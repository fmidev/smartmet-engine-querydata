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
#include "ParameterOptions.h"
#include "ParameterTranslations.h"
#include "ValidTimeList.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <gis/CoordinateMatrix.h>
#include <newbase/NFmiParameterName.h>
#include <spine/Thread.h>
#include <timeseries/TimeSeriesInclude.h>

#include <list>

class NFmiArea;
class NFmiMetTime;
class NFmiPoint;
class NFmiFastQueryInfo;

namespace Fmi
{
class DEM;
class LandCover;
class SpatialReference;
}  // namespace Fmi

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class QImpl : private boost::noncopyable, public boost::enable_shared_from_this<QImpl>
{
 public:
  ~QImpl();

  QImpl() = delete;
  QImpl(SharedModel theModel);
  QImpl(const std::vector<SharedModel>& theModels);

  // Avoid using this as much as possible
  boost::shared_ptr<NFmiFastQueryInfo> info();

  boost::shared_ptr<ValidTimeList> validTimes() const;

  const std::string& levelName() const;
  FmiLevelType levelType() const;
  bool isClimatology() const;
  bool isFullGrid() const;
  bool isRelativeUV() const;

  std::size_t hashValue() const;
  std::size_t gridHashValue() const;

  NFmiPoint validPoint(const NFmiPoint& theLatLon, double theMaxDist) const;

  // API correspondence with NFmiFastQueryInfo

  const NFmiMetTime& originTime() const;
  boost::posix_time::ptime modificationTime() const;
  boost::posix_time::ptime expirationTime() const;
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

  void resetLocation();
  bool firstLocation();
  bool nextLocation();
  NFmiPoint worldXY() const;
  NFmiPoint latLon() const;

  const Fmi::SpatialReference& SpatialReference() const;

  // Using different names to locate uses cases more easily
  Fmi::CoordinateMatrix CoordinateMatrix() const;
  Fmi::CoordinateMatrix FullCoordinateMatrix() const;

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
                    int theMaxMinuteGap = 0);
  float interpolateAtPressure(const NFmiPoint& theLatLon,
                              const NFmiMetTime& theTime,
                              float thePressure,
                              int theMaxMinuteGap = 0);
  float interpolateAtHeight(const NFmiPoint& theLatLon,
                            const NFmiMetTime& theTime,
                            float theHeight,
                            int theMaxMinuteGap = 0);

  NFmiPoint latLon(long theIndex) const;

  bool isSubParamUsed() const;
  void setIsSubParamUsed(bool theState);

  unsigned long paramIndex() const;
  bool paramIndex(unsigned long theIndex);
  unsigned long levelIndex() const;
  bool levelIndex(unsigned long theIndex);
  unsigned long timeIndex() const;
  bool timeIndex(unsigned long theIndex);
  unsigned long locationIndex() const;
  bool locationIndex(unsigned long theIndex);

  bool calcTimeCache(NFmiQueryInfo& theTargetInfo, std::vector<NFmiTimeCache>& theTimeCache);
  NFmiTimeCache calcTimeCache(const NFmiMetTime& theTime);
  float cachedInterpolation(const NFmiTimeCache& theTimeCache);
  float cachedInterpolation(const NFmiLocationCache& theLocationCache);
  float cachedInterpolation(const NFmiLocationCache& theLocationCache,
                            const NFmiTimeCache& theTimeCache);
  NFmiDataMatrix<float> landscapeCachedInterpolation(
      const NFmiDataMatrix<NFmiLocationCache>& theLocationCache,
      const NFmiTimeCache& theTimeCache,  // Unset value (NoValue())
                                          // for native (currently
                                          // active) time
      const NFmiDataMatrix<float>& theDEMValues,
      const NFmiDataMatrix<bool>& theWaterFlags);
  bool calcLatlonCachePoints(NFmiQueryInfo& theTargetInfo,
                             NFmiDataMatrix<NFmiLocationCache>& theLocationCache);

  // Generic access to grid values:
  NFmiDataMatrix<float> values(const NFmiDataMatrix<float>& theDEMValues = NFmiDataMatrix<float>(),
                               const NFmiDataMatrix<bool>& theWaterFlags = NFmiDataMatrix<bool>());

  NFmiDataMatrix<float> values(const NFmiMetTime& theInterpolatedTime,
                               const NFmiDataMatrix<float>& theDEMValues = NFmiDataMatrix<float>(),
                               const NFmiDataMatrix<bool>& theWaterFlags = NFmiDataMatrix<bool>());

  // Needed for metaparameters:
  NFmiDataMatrix<float> values(const Spine::Parameter& theParam,
                               const boost::posix_time::ptime& theInterpolatedTime,
                               const NFmiDataMatrix<float>& theDEMValues = NFmiDataMatrix<float>(),
                               const NFmiDataMatrix<bool>& theWaterFlags = NFmiDataMatrix<bool>());

  // For arbitrary coordinates:

  NFmiDataMatrix<float> values(const Fmi::CoordinateMatrix& theLatlonMatrix,
                               const NFmiMetTime& theTime,
                               float P = kFloatMissing,
                               float H = kFloatMissing);

  NFmiDataMatrix<float> croppedValues(
      int x1,
      int y1,
      int x2,
      int y2,
      const NFmiDataMatrix<float>& theDEMValues = NFmiDataMatrix<float>(),
      const NFmiDataMatrix<bool>& theWaterFlags = NFmiDataMatrix<bool>()) const;

  NFmiDataMatrix<float> pressureValues(const NFmiMetTime& theInterpolatedTime,
                                       float wantedPressureLevel);

  NFmiDataMatrix<float> pressureValues(const NFmiGrid& theWantedGrid,
                                       const NFmiMetTime& theInterpolatedTime,
                                       float wantedPressureLevel);

  NFmiDataMatrix<float> pressureValues(const NFmiGrid& theWantedGrid,
                                       const NFmiMetTime& theInterpolatedTime,
                                       float wantedPressureLevel,
                                       bool relative_uv);

  NFmiDataMatrix<float> gridValues(const NFmiGrid& theWantedGrid,
                                   const NFmiMetTime& theInterpolatedTime,
                                   bool relative_uv);

  NFmiDataMatrix<float> heightValues(const NFmiGrid& theWantedGrid,
                                     const NFmiMetTime& theInterpolatedTime,
                                     float wantedHeightLevel,
                                     bool relative_uv);

  // Gridded landscaping; Load dem values and water flags for native (sub)grid or for given
  // locations

  bool loadDEMAndWaterFlags(const Fmi::DEM& theDEM,
                            const Fmi::LandCover& theLandCover,
                            double theResolution,
                            const NFmiDataMatrix<NFmiLocationCache>& theLocationCache,
                            NFmiDataMatrix<float>& theDemMatrix,
                            NFmiDataMatrix<bool>& theWaterFlagMatrix,
                            int x1 = 0,
                            int y1 = 0,
                            int x2 = 0,
                            int y2 = 0) const;

  // sample data into a new projection

  boost::shared_ptr<QImpl> sample(const Spine::Parameter& theParameter,
                                  const boost::posix_time::ptime& theTime,
                                  const Fmi::SpatialReference& theCrs,
                                  double theXmin,
                                  double theYmin,
                                  double theXmax,
                                  double theYmax,
                                  double theResolution,
                                  const Fmi::DEM& theDem,
                                  const Fmi::LandCover& theLandCover);

  // one location, one timestep
  TS::Value value(const ParameterOptions& opt,
                                 const boost::local_time::local_date_time& ldt);
  TS::Value valueAtPressure(const ParameterOptions& opt,
                                           const boost::local_time::local_date_time& ldt,
                                           float pressure);
  TS::Value valueAtHeight(const ParameterOptions& opt,
                                         const boost::local_time::local_date_time& ldt,
                                         float height);
  // one location, many timesteps
  TS::TimeSeriesPtr values(const ParameterOptions& param,
                                          const TS::TimeSeriesGenerator::LocalTimeList& tlist);
  TS::TimeSeriesPtr valuesAtPressure(
      const ParameterOptions& param,
      const TS::TimeSeriesGenerator::LocalTimeList& tlist,
      float pressure);
  TS::TimeSeriesPtr valuesAtHeight(
      const ParameterOptions& param,
      const TS::TimeSeriesGenerator::LocalTimeList& tlist,
      float height);
  // many locations (indexmask), many timesteps
  TS::TimeSeriesGroupPtr values(
      const ParameterOptions& param,
      const NFmiIndexMask& indexmask,
      const TS::TimeSeriesGenerator::LocalTimeList& tlist);
  TS::TimeSeriesGroupPtr valuesAtPressure(
      const ParameterOptions& param,
      const NFmiIndexMask& indexmask,
      const TS::TimeSeriesGenerator::LocalTimeList& tlist,
      float pressure);
  TS::TimeSeriesGroupPtr valuesAtHeight(
      const ParameterOptions& param,
      const NFmiIndexMask& indexmask,
      const TS::TimeSeriesGenerator::LocalTimeList& tlist,
      float height);
  // many locations (llist), many timesteps
  TS::TimeSeriesGroupPtr values(
      const ParameterOptions& param,
      const Spine::LocationList& llist,
      const TS::TimeSeriesGenerator::LocalTimeList& tlist,
      const double& maxdistance);
  TS::TimeSeriesGroupPtr valuesAtPressure(
      const ParameterOptions& param,
      const Spine::LocationList& llist,
      const TS::TimeSeriesGenerator::LocalTimeList& tlist,
      const double& maxdistance,
      float pressure);
  TS::TimeSeriesGroupPtr valuesAtHeight(
      const ParameterOptions& param,
      const Spine::LocationList& llist,
      const TS::TimeSeriesGenerator::LocalTimeList& tlist,
      const double& maxdistance,
      float height);

  bool selectLevel(double theLevel);

  bool needsGlobeWrap() const;

  void setParameterTranslations(boost::shared_ptr<ParameterTranslations> translations)
  {
    itsParameterTranslations = translations;
  }

 private:
  NFmiDataMatrix<float> calculatedValues(const Spine::Parameter& theParam,
                                         const boost::posix_time::ptime& theInterpolatedTime);

  TS::Value dataIndependentValue(const ParameterOptions& opt,
                                                const boost::local_time::local_date_time& ldt,
                                                double levelResult);

  TS::Value dataValue(const ParameterOptions& opt,
                                     const NFmiPoint& latlon,
                                     const boost::local_time::local_date_time& ldt);
  TS::Value dataValueAtPressure(const ParameterOptions &opt,
											   const NFmiPoint &latlon,
											   const boost::local_time::local_date_time &ldt,
											   float pressure);
  TS::Value dataValueAtHeight(const ParameterOptions &opt,
											 const NFmiPoint &latlon,
											 const boost::local_time::local_date_time &ldt,
											 float height);

  std::vector<SharedModel> itsModels;
  std::vector<SharedInfo> itsInfos;  // used only in destructor and MultiInfo constructor
  boost::shared_ptr<NFmiFastQueryInfo> itsInfo;    // or NFmiMultiQueryInfo
  boost::shared_ptr<ValidTimeList> itsValidTimes;  // collective over all datas
  std::size_t itsHashValue;

  boost::shared_ptr<ParameterTranslations> itsParameterTranslations;

};  // class QImpl

using Q = boost::shared_ptr<QImpl>;
using QList = std::list<Q>;

inline std::size_t hash_value(const Q& theQ)
{
  return theQ->hashValue();
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
