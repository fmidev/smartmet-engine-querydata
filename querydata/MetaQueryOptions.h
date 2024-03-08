// ======================================================================
/*!
 * \brief Interface for metadata query options
 */
// ======================================================================

#pragma once

#include <list>
#include <string>

#include <newbase/NFmiPoint.h>

#include <macgyver/DateTime.h>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class MetaQueryOptions
{
 public:
  struct BBox
  {
    BBox() : ul(0, 0), ur(0, 0), bl(0, 0), br(0, 0) {}
    BBox(const NFmiPoint& newbl,
         const NFmiPoint& newbr,
         const NFmiPoint& newul,
         const NFmiPoint& newur)
        : ul(newul), ur(newur), bl(newbl), br(newbr)
    {
    }

    BBox(const NFmiPoint& newbl, const NFmiPoint& newur)
        : ul(NFmiPoint(newbl.X(), newur.Y())),
          ur(newur),
          bl(newbl),
          br(NFmiPoint(newur.X(), newbl.Y()))
    {
    }

    NFmiPoint ul;
    NFmiPoint ur;
    NFmiPoint bl;
    NFmiPoint br;
  };

  MetaQueryOptions();

  void setProducer(const std::string& producer);

  bool hasProducer() const;

  std::string getProducer() const;

  void setOriginTime(const Fmi::DateTime& originTime);

  bool hasOriginTime() const;

  Fmi::DateTime getOriginTime() const;

  void setFirstTime(const Fmi::DateTime& firstTime);

  bool hasFirstTime() const;

  Fmi::DateTime getFirstTime() const;

  void setLastTime(const Fmi::DateTime& lastTime);

  bool hasLastTime() const;

  Fmi::DateTime getLastTime() const;

  void addParameter(const std::string& parameter);

  bool hasParameters() const;

  std::list<std::string> getParameters() const;

  void setBoundingBox(const NFmiPoint& ul,
                      const NFmiPoint& ur,
                      const NFmiPoint& bl,
                      const NFmiPoint& br);

  void setBoundingBox(const NFmiPoint& bl, const NFmiPoint& ur);

  bool hasBoundingBox() const;

  BBox getBoundingBox() const;

  void addLevelType(const std::string& type);

  bool hasLevelTypes() const;

  std::list<std::string> getLevelTypes() const;

  void addLevelValue(float value);

  bool hasLevelValues() const;

  std::list<float> getLevelValues() const;

 private:
  std::string itsProducer;

  Fmi::DateTime itsOriginTime;

  Fmi::DateTime itsFirstTime;

  Fmi::DateTime itsLastTime;

  std::list<std::string> itsParameters;

  BBox itsBoundingBox;

  std::list<std::string> itsLevelTypes;

  std::list<float> itsLevelValues;

  bool itsHasProducer = false;

  bool itsHasOriginTime = false;

  bool itsHasFirstTime = false;

  bool itsHasLastTime = false;

  bool itsHasParameters = false;

  bool itsHasBoundingBox = false;

  bool itsHasLevelTypes = false;

  bool itsHasLevelValues = false;
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
