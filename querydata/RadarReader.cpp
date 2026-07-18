// ======================================================================
/*!
 * \brief Implementation of radar GeoTIFF / ODIM HDF5 -> querydata reading.
 */
// ======================================================================

#include "RadarReader.h"

#include "Hdf5File.h"

#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal_priv.h>

#include <gis/ProjInfo.h>
#include <gis/SpatialReference.h>
#include <macgyver/Exception.h>

#include <newbase/NFmiArea.h>
#include <newbase/NFmiDataIdent.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiHPlaceDescriptor.h>
#include <newbase/NFmiLevel.h>
#include <newbase/NFmiLevelBag.h>
#include <newbase/NFmiMetTime.h>
#include <newbase/NFmiParam.h>
#include <newbase/NFmiParamBag.h>
#include <newbase/NFmiParamDescriptor.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiProducer.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeDescriptor.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiVPlaceDescriptor.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
namespace
{
// ---------------------------------------------------------------------------
// The following decode helpers are lifted from qdless' GdalRasterSource so
// that we parse FMI radar GeoTIFFs exactly the way the interactive viewer does.
// ---------------------------------------------------------------------------

// Parse "YYYYMMDDhhmm[ss]" -> NFmiMetTime (UTC), or nullopt on failure.
std::optional<NFmiMetTime> parseUtcStamp(const std::string& s)
{
  if (s.size() < 12)
    return std::nullopt;
  try
  {
    short yy = static_cast<short>(std::stoi(s.substr(0, 4)));
    short mm = static_cast<short>(std::stoi(s.substr(4, 2)));
    short dd = static_cast<short>(std::stoi(s.substr(6, 2)));
    short h = static_cast<short>(std::stoi(s.substr(8, 2)));
    short mi = static_cast<short>(std::stoi(s.substr(10, 2)));
    short se = (s.size() >= 14) ? static_cast<short>(std::stoi(s.substr(12, 2))) : 0;
    // 1-minute time step so sub-hourly stamps are not snapped to the hour;
    // NearestMetTime hardcodes SetSec(0), so re-apply seconds afterwards.
    NFmiMetTime r(yy, mm, dd, h, mi, /*sec=*/0, /*timeStep=*/1);
    r.SetSec(se);
    return r;
  }
  catch (...)
  {
    return std::nullopt;
  }
}

NFmiMetTime parseTimeFromName(const std::string& filename)
{
  const std::string base = std::filesystem::path(filename).filename().string();
  // `<origin>_<validtime>_<product>` -> prefer the second (valid) stamp.
  static const std::regex re2(R"(^(\d{12,14})_(\d{12,14})_)");
  std::smatch m2;
  if (std::regex_search(base, m2, re2))
    if (auto t = parseUtcStamp(m2[2]))
      return *t;
  static const std::regex re(R"((\d{12,14}))");
  std::smatch m;
  if (std::regex_search(base, m, re))
    if (auto t = parseUtcStamp(m[1]))
      return *t;
  std::error_code ec;
  auto ftime = std::filesystem::last_write_time(filename, ec);
  if (ec)
    return NFmiMetTime();
  const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
  const auto t = std::chrono::system_clock::to_time_t(sctp);
  std::tm utc{};
  gmtime_r(&t, &utc);
  return NFmiMetTime(static_cast<short>(utc.tm_year + 1900),
                     static_cast<short>(utc.tm_mon + 1),
                     static_cast<short>(utc.tm_mday),
                     static_cast<short>(utc.tm_hour),
                     static_cast<short>(utc.tm_min),
                     static_cast<short>(utc.tm_sec));
}

std::string extractLabel(const std::string& filename)
{
  std::string base = std::filesystem::path(filename).stem().string();
  static const std::regex re(R"(^(?:\d{12,14}_){1,2}(.*)$)");
  std::smatch m;
  if (std::regex_search(base, m, re))
    return m[1];
  return base;
}

struct MetaItem
{
  std::string value;
  std::string unit;
};

std::string unescapeXml(std::string s)
{
  static const std::pair<std::string, std::string> kEntities[] = {
      {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
  for (const auto& [from, to] : kEntities)
  {
    std::string::size_type pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
      s.replace(pos, from.size(), to);
      pos += to.size();
    }
  }
  return s;
}

std::map<std::string, MetaItem> parseGdalMetadata(const std::string& xml)
{
  std::map<std::string, MetaItem> out;
  if (xml.empty())
    return out;
  static const std::regex itemRe(R"(<Item\s+([^>]*?)>([^<]*)</Item>)", std::regex::ECMAScript);
  static const std::regex nameRe(R"x(name="([^"]*)")x");
  static const std::regex unitRe(R"x(unit="([^"]*)")x");
  auto begin = std::sregex_iterator(xml.begin(), xml.end(), itemRe);
  for (auto it = begin; it != std::sregex_iterator(); ++it)
  {
    const std::string attrs = (*it)[1].str();
    const std::string value = unescapeXml((*it)[2].str());
    std::smatch nm;
    if (!std::regex_search(attrs, nm, nameRe))
      continue;
    MetaItem mi;
    mi.value = value;
    std::smatch um;
    if (std::regex_search(attrs, um, unitRe))
      mi.unit = um[1].str();
    out[nm[1].str()] = std::move(mi);
  }
  return out;
}

int quantityToParamId(const std::string& quantity)
{
  std::string q = quantity;
  std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return std::tolower(c); });
  if (q.find("precipitation accumulation") != std::string::npos ||
      q.find("precipitation amount") != std::string::npos ||
      q.find("rainfall accumulation") != std::string::npos)
    return kFmiPrecipitationAmount;
  if (q.find("precipitation rate") != std::string::npos ||
      q.find("rainfall rate") != std::string::npos ||
      q.find("precipitation intensity") != std::string::npos)
    return kFmiPrecipitationRate;
  // "corrected reflectivity" must be tested before the generic "reflectivity",
  // and maps to kFmiCorrectedReflectivity (126) to match the ODIM DBZH path;
  // otherwise GeoTIFF composites land on kFmiReflectivity (1103, uncorrected)
  // and WMS layers asking for CorrectedReflectivity find no data.
  if (q.find("corrected reflectivity") != std::string::npos)
    return kFmiCorrectedReflectivity;
  if (q.find("reflectivity") != std::string::npos)
    return kFmiReflectivity;
  if (q.find("echo top") != std::string::npos)
    return kFmiEchoTop;
  if (q.find("radial velocity") != std::string::npos)
    return kFmiRadialVelocity;
  return 0;
}

int labelToParamId(const std::string& label)
{
  if (label.find("rr1h") != std::string::npos || label.find("rr3h") != std::string::npos ||
      label.find("rr12h") != std::string::npos || label.find("rr24h") != std::string::npos ||
      label.find("rrate") != std::string::npos)
    return kFmiPrecipitationAmount;
  if (label.find("dbz") != std::string::npos || label.find("refl") != std::string::npos)
    return kFmiReflectivity;
  return 0;
}

struct Guard
{
  GDALDataset* p;
  ~Guard()
  {
    if (p != nullptr)
      GDALClose(p);
  }
};

// ---------------------------------------------------------------------------

std::shared_ptr<NFmiQueryData> readGeoTiff(const std::filesystem::path& path)
{
  const std::string filename = path.string();

  GDALAllRegister();
  auto* ds = static_cast<GDALDataset*>(GDALOpen(filename.c_str(), GA_ReadOnly));
  if (ds == nullptr)
    throw Fmi::Exception(BCP, "GDAL failed to open radar GeoTIFF: " + filename);
  Guard guard{ds};

  const auto nx = static_cast<std::size_t>(ds->GetRasterXSize());
  const auto ny = static_cast<std::size_t>(ds->GetRasterYSize());
  if (nx == 0 || ny == 0)
    throw Fmi::Exception(BCP, "Empty radar raster: " + filename);

  double gt[6] = {};
  if (ds->GetGeoTransform(gt) != CE_None)
    throw Fmi::Exception(BCP, "Radar GeoTIFF has no geotransform: " + filename);
  if (std::abs(gt[2]) > 1e-9 || std::abs(gt[4]) > 1e-9)
    throw Fmi::Exception(BCP, "Rotated radar GeoTIFFs are not supported: " + filename);

  const double originX = gt[0];
  const double pixelW = gt[1];
  const double originY = gt[3];
  const double pixelH = gt[5];  // negative for north-up
  const double x0 = originX;
  const double y0 = originY;
  const double x1 = originX + static_cast<double>(nx) * pixelW;
  const double y1 = originY + static_cast<double>(ny) * pixelH;
  const double minX = std::min(x0, x1);
  const double maxX = std::max(x0, x1);
  const double minY = std::min(y0, y1);
  const double maxY = std::max(y0, y1);

  const OGRSpatialReference* osr = ds->GetSpatialRef();
  OGRSpatialReference assumedWgs84;
  if (osr == nullptr)
  {
    const bool geographic = minX >= -360.0 && maxX <= 360.0 && minY >= -90.001 && maxY <= 90.001;
    if (!geographic)
      throw Fmi::Exception(
          BCP, "Radar GeoTIFF has no projection and a non-geographic extent: " + filename);
    assumedWgs84.SetWellKnownGeogCS("WGS84");
    assumedWgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    osr = &assumedWgs84;
  }

  Fmi::SpatialReference sr(*osr);
  // CreateFromBBox logs to stderr for latitudes PROJ rejects; silence it and
  // rely on the null check. For EPSG:3067 this yields a native
  // NFmiTransverseMercatorArea.
  std::unique_ptr<NFmiArea> area;
  CPLPushErrorHandler(CPLQuietErrorHandler);
  area.reset(NFmiArea::CreateFromBBox(sr, NFmiPoint(minX, minY), NFmiPoint(maxX, maxY)));
  CPLPopErrorHandler();
  if (!area)
    throw Fmi::Exception(BCP, "Failed to construct projection for radar GeoTIFF: " + filename);

  // ODIM-style GDAL_METADATA XML blob (TIFF tag 42112).
  std::map<std::string, MetaItem> meta;
  if (const char* blob = ds->GetMetadataItem("GDAL_METADATA"))
    meta = parseGdalMetadata(blob);

  auto metaItem = [&](std::initializer_list<const char*> keys) -> std::string
  {
    for (const char* k : keys)
      if (const char* v = ds->GetMetadataItem(k); v != nullptr && *v != '\0')
        return v;
    return {};
  };

  // Valid time: Observation time -> ForecastTimestamp -> filename/mtime.
  std::optional<NFmiMetTime> vt;
  if (auto it = meta.find("Observation time"); it != meta.end())
    vt = parseUtcStamp(it->second.value);
  if (!vt)
    vt = parseUtcStamp(metaItem({"ForecastTimestamp"}));
  const NFmiMetTime validTime = vt ? *vt : parseTimeFromName(filename);

  // Origin time: ODIM Timestamp, or the first of two leading filename stamps,
  // else the valid time.
  NFmiMetTime originTime = validTime;
  if (auto ot = parseUtcStamp(metaItem({"Timestamp"})))
    originTime = *ot;
  else
  {
    const std::string base = path.filename().string();
    static const std::regex re2(R"(^(\d{12,14})_(\d{12,14})_)");
    std::smatch m2;
    if (std::regex_search(base, m2, re2))
      if (auto o = parseUtcStamp(m2[1]))
        originTime = *o;
  }

  // Parameter naming: Quantity -> ODIM quantity -> filename label -> enum.
  const std::string label = extractLabel(filename);
  int paramId = 0;
  std::string paramName;
  std::string odimQuantity = metaItem({"dataset1_data1_what_quantity", "what_quantity"});
  if (auto it = meta.find("Quantity"); it != meta.end())
  {
    paramName = it->second.value;
    paramId = quantityToParamId(paramName);
  }
  else if (!odimQuantity.empty())
  {
    paramName = odimQuantity;
    paramId = quantityToParamId(odimQuantity);
  }
  else
  {
    paramId = labelToParamId(label);
    if (paramId == 0)
    {
      NFmiEnumConverter conv;
      int id = conv.ToEnum(label.c_str());
      if (id != 0)
        paramId = id;
    }
    if (paramId != 0)
    {
      NFmiEnumConverter conv;
      paramName = conv.ToString(paramId);
    }
    else
      paramName = label.empty() ? std::string{"RadarValue"} : label;
  }
  // Querydata needs a numeric parameter id; synthesize one if unresolved.
  if (paramId == 0)
    paramId = 1;

  // Gain / offset / nodata / undetect.
  double gain = 1.0;
  double offset = 0.0;
  double nodata = 0.0;
  double undetect = 0.0;
  bool hasGain = false;
  bool hasOffset = false;
  bool hasNodata = false;
  bool hasUndetect = false;
  auto fetchDouble = [&](const char* key, double& out, bool& has)
  {
    auto it = meta.find(key);
    if (it == meta.end())
      return;
    try
    {
      out = std::stod(it->second.value);
      has = true;
    }
    catch (...)
    {
    }
  };
  fetchDouble("Gain", gain, hasGain);
  fetchDouble("Offset", offset, hasOffset);
  fetchDouble("Nodata", nodata, hasNodata);
  fetchDouble("Undetect", undetect, hasUndetect);
  auto fetchMetaDouble = [&](std::initializer_list<const char*> keys, double& out, bool& has)
  {
    if (has)
      return;
    const std::string v = metaItem(keys);
    if (v.empty())
      return;
    try
    {
      out = std::stod(v);
      has = true;
    }
    catch (...)
    {
    }
  };
  fetchMetaDouble({"dataset1_data1_what_gain", "what_gain"}, gain, hasGain);
  fetchMetaDouble({"dataset1_data1_what_offset", "what_offset"}, offset, hasOffset);
  fetchMetaDouble({"dataset1_data1_what_nodata", "what_nodata"}, nodata, hasNodata);
  fetchMetaDouble({"dataset1_data1_what_undetect", "what_undetect"}, undetect, hasUndetect);
  if (!hasGain)
    gain = 1.0;
  if (!hasOffset)
    offset = 0.0;
  if (!hasNodata)
  {
    int has = 0;
    double v = ds->GetRasterBand(1)->GetNoDataValue(&has);
    if (has != 0)
    {
      nodata = v;
      hasNodata = true;
    }
  }

  // Read the raster band as float (GDAL converts Byte / UInt16 uniformly).
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (band == nullptr)
    throw Fmi::Exception(BCP, "Radar GeoTIFF has no raster band: " + filename);
  std::vector<float> raw(nx * ny);
  CPLErr err = band->RasterIO(GF_Read,
                              0,
                              0,
                              static_cast<int>(nx),
                              static_cast<int>(ny),
                              raw.data(),
                              static_cast<int>(nx),
                              static_cast<int>(ny),
                              GDT_Float32,
                              0,
                              0);
  if (err != CE_None)
    throw Fmi::Exception(BCP, "RasterIO failed for radar GeoTIFF: " + filename);

  // Build the querydata descriptors (single param / level / time).
  NFmiParam param(static_cast<unsigned long>(paramId), paramName);
  param.InterpolationMethod(kLinearly);
  NFmiParamBag pbag;
  pbag.Add(NFmiDataIdent(param));
  NFmiParamDescriptor pdesc(pbag);

  NFmiTimeList tlist;
  tlist.Add(new NFmiMetTime(validTime));
  NFmiTimeDescriptor tdesc(originTime, tlist);

  NFmiLevelBag lbag;
  lbag.AddLevel(NFmiLevel(kFmiAnyLevelType, 0));
  NFmiVPlaceDescriptor vdesc(lbag);

  NFmiGrid grid(area.get(), static_cast<unsigned long>(nx), static_cast<unsigned long>(ny));
  NFmiHPlaceDescriptor hdesc(grid);

  NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
  std::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(qi));
  if (!data)
    throw Fmi::Exception(BCP, "Failed to allocate querydata for radar GeoTIFF: " + filename);

  NFmiFastQueryInfo info(data.get());
  info.SetProducer(NFmiProducer(1014, "RADAR"));
  info.FirstParam();
  info.FirstLevel();
  info.FirstTime();

  // Fill the grid. GDAL rows run north (row 0) to south; newbase grids are
  // bottom-left origin, so flip the row index. nodata -> missing; undetect ->
  // physical floor (gain*0+offset), matching h5toqd.
  std::size_t pos = 0;
  for (info.ResetLocation(); info.NextLocation(); ++pos)
  {
    const std::size_t i = pos % nx;
    const std::size_t j = pos / nx;
    const std::size_t srcRow = ny - 1 - j;
    const double v = raw[srcRow * nx + i];
    float out = kFloatMissing;
    if (hasNodata && v == nodata)
      out = kFloatMissing;
    else if (hasUndetect && v == undetect)
      out = static_cast<float>(offset);
    else
      out = static_cast<float>(gain * v + offset);
    info.FloatValue(out);
  }

  return data;
}

// ---------------------------------------------------------------------------
// ODIM HDF5 (Cartesian COMP / IMAGE / CVOL), following qdtools' h5toqd.
// ---------------------------------------------------------------------------

int operaQuantityToParamId(const std::string& product, const std::string& quantity)
{
  if (quantity == "DBZH" || quantity == "DBZHC")
    return kFmiCorrectedReflectivity;
  if (quantity == "TH" || quantity == "DBZ" || quantity == "DBZV")
    return kFmiReflectivity;
  if (quantity == "VRAD" || quantity == "VRADH" || quantity == "VRADV" || quantity == "VRADDH")
    return kFmiRadialVelocity;
  if (quantity == "WRAD" || quantity == "W")
    return kFmiSpectralWidth;
  if (quantity == "RATE")
    return kFmiPrecipitationRate;
  if (quantity == "ACRR")
    return kFmiPrecipitationAmount;
  if (quantity == "HGHT" || product == "ETOP")
    return kFmiEchoTop;
  return 0;
}

std::shared_ptr<NFmiQueryData> readOdim(const std::filesystem::path& path)
{
  Fmi::HDF5::Hdf5File file(path.string());

  const std::string object = file.get_attribute<std::string>("/what", "object");
  if (object != "COMP" && object != "IMAGE" && object != "CVOL")
    throw Fmi::Exception(BCP,
                         "Only Cartesian ODIM (COMP/IMAGE/CVOL) is supported; got object=" +
                             object + ": " + path.string());

  // Projection + geographic corners (see h5toqd create_hdesc). projdef is the
  // target CRS, its inverse the lon/lat CRS the corners are given in.
  std::string projdef = file.get_attribute<std::string>("/where", "projdef");
  Fmi::ProjInfo proj(projdef);
  proj.erase("x_0");
  proj.erase("y_0");
  projdef = proj.projStr();
  const std::string sphere = proj.inverseProjStr();

  const long xsize = file.get_attribute<long>("/where", "xsize");
  const long ysize = file.get_attribute<long>("/where", "ysize");

  std::unique_ptr<NFmiArea> area;
  CPLPushErrorHandler(CPLQuietErrorHandler);
  if (!file.is_attribute<double>("/where", "LL_lon"))
  {
    // Latvian-style corners
    const double LR_lon = file.get_attribute<double>("/where", "LR_lon");
    const double LR_lat = file.get_attribute<double>("/where", "LR_lat");
    const double UL_lon = file.get_attribute<double>("/where", "UL_lon");
    const double UL_lat = file.get_attribute<double>("/where", "UL_lat");
    area.reset(NFmiArea::CreateFromReverseCorners(
        projdef, sphere, NFmiPoint(UL_lon, UL_lat), NFmiPoint(LR_lon, LR_lat)));
  }
  else
  {
    // FMI-style corners
    const double LL_lon = file.get_attribute<double>("/where", "LL_lon");
    const double LL_lat = file.get_attribute<double>("/where", "LL_lat");
    const double UR_lon = file.get_attribute<double>("/where", "UR_lon");
    const double UR_lat = file.get_attribute<double>("/where", "UR_lat");
    area.reset(NFmiArea::CreateFromCorners(
        projdef, sphere, NFmiPoint(LL_lon, LL_lat), NFmiPoint(UR_lon, UR_lat)));
  }
  CPLPopErrorHandler();
  if (!area)
    throw Fmi::Exception(BCP, "Failed to construct projection for ODIM file: " + path.string());

  // Single Cartesian data slice. Numbered layout (/dataset1/data1, FMI Rack)
  // or unnumbered (/dataset1/data). read_dataset(group) resolves group/data,
  // and get_attribute_recursive walks up to /dataset1/what either way.
  std::string dpath = "/dataset1/data1";
  if (!file.is_group(dpath))
    dpath = "/dataset1";
  const std::string product = file.get_attribute_recursive<std::string>(dpath, "what", "product");
  const std::string quantity = file.get_attribute_recursive<std::string>(dpath, "what", "quantity");
  const std::optional<double> nodata =
      file.get_optional_attribute_recursive<double>(dpath, "what", "nodata");
  const std::optional<double> undetect =
      file.get_optional_attribute_recursive<double>(dpath, "what", "undetect");
  const std::optional<double> gain =
      file.get_optional_attribute_recursive<double>(dpath, "what", "gain");
  const std::optional<double> offset =
      file.get_optional_attribute_recursive<double>(dpath, "what", "offset");
  const double g = gain.value_or(1.0);
  const double o = offset.value_or(0.0);

  int paramId = operaQuantityToParamId(product, quantity);
  std::string paramName;
  if (paramId != 0)
  {
    NFmiEnumConverter conv;
    paramName = conv.ToString(paramId);
  }
  else
  {
    paramId = 1;
    paramName = quantity.empty() ? std::string{"RadarValue"} : quantity;
  }

  // Valid time: dataset end time, else the nominal /what time.
  std::optional<std::string> sd =
      file.get_optional_attribute<std::string>("/dataset1/what", "enddate");
  if (!sd)
    sd = file.get_optional_attribute<std::string>("/what", "date");
  std::optional<std::string> st =
      file.get_optional_attribute<std::string>("/dataset1/what", "endtime");
  if (!st)
    st = file.get_optional_attribute<std::string>("/what", "time");
  NFmiMetTime validTime;
  if (sd && st)
    if (auto t = parseUtcStamp(*sd + *st))
      validTime = *t;
  NFmiMetTime originTime = validTime;
  {
    auto od = file.get_optional_attribute<std::string>("/what", "date");
    auto ot = file.get_optional_attribute<std::string>("/what", "time");
    if (od && ot)
      if (auto t = parseUtcStamp(*od + *ot))
        originTime = *t;
  }

  const std::vector<int> values = file.read_dataset<int>(dpath);
  const std::size_t nx = static_cast<std::size_t>(xsize);
  const std::size_t ny = static_cast<std::size_t>(ysize);
  if (values.size() < nx * ny)
    throw Fmi::Exception(BCP, "ODIM dataset smaller than xsize*ysize: " + path.string());

  NFmiParam param(static_cast<unsigned long>(paramId), paramName);
  param.InterpolationMethod(kLinearly);
  NFmiParamBag pbag;
  pbag.Add(NFmiDataIdent(param));
  NFmiParamDescriptor pdesc(pbag);

  NFmiTimeList tlist;
  tlist.Add(new NFmiMetTime(validTime));
  NFmiTimeDescriptor tdesc(originTime, tlist);

  NFmiLevelBag lbag;
  lbag.AddLevel(NFmiLevel(kFmiAnyLevelType, 0));
  NFmiVPlaceDescriptor vdesc(lbag);

  NFmiGrid grid(area.get(), static_cast<unsigned long>(nx), static_cast<unsigned long>(ny));
  NFmiHPlaceDescriptor hdesc(grid);

  NFmiFastQueryInfo qi(pdesc, tdesc, hdesc, vdesc);
  std::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(qi));
  if (!data)
    throw Fmi::Exception(BCP, "Failed to allocate querydata for ODIM file: " + path.string());

  NFmiFastQueryInfo info(data.get());
  info.SetProducer(NFmiProducer(1014, "RADAR"));
  info.FirstParam();
  info.FirstLevel();
  info.FirstTime();

  // Same north-up -> bottom-up flip and nodata/undetect handling as GeoTIFF.
  std::size_t pos = 0;
  for (info.ResetLocation(); info.NextLocation(); ++pos)
  {
    const std::size_t i = pos % nx;
    const std::size_t j = pos / nx;
    const std::size_t srcRow = ny - 1 - j;
    const double v = values[srcRow * nx + i];
    float out = kFloatMissing;
    if (nodata && v == *nodata)
      out = kFloatMissing;
    else if (undetect && v == *undetect)
      out = static_cast<float>(o);
    else
      out = static_cast<float>(g * v + o);
    info.FloatValue(out);
  }

  return data;
}

}  // namespace

// ---------------------------------------------------------------------------

RadarFormat detectRadarFormat(const std::filesystem::path& path)
{
  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  if (ext == ".tif" || ext == ".tiff")
    return RadarFormat::GeoTiff;
  if (ext == ".h5" || ext == ".hdf")
    return RadarFormat::Odim;
  if (ext == ".sqd")
    return RadarFormat::QueryData;
  return RadarFormat::QueryData;  // unknown -> treat as needing no conversion
}

std::shared_ptr<NFmiQueryData> readRadarFile(const std::filesystem::path& path, RadarFormat format)
{
  try
  {
    const RadarFormat fmt = (format == RadarFormat::Auto) ? detectRadarFormat(path) : format;
    switch (fmt)
    {
      case RadarFormat::GeoTiff:
        return readGeoTiff(path);
      case RadarFormat::Odim:
        return readOdim(path);
      case RadarFormat::QueryData:
      case RadarFormat::Auto:
      default:
        throw Fmi::Exception(BCP, "Not a radar raster file requiring conversion: " + path.string());
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Reading radar file failed: " + path.string());
  }
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
