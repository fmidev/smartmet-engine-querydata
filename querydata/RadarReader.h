// ======================================================================
/*!
 * \brief Reading radar GeoTIFF / ODIM HDF5 files into in-memory querydata.
 *
 * A monochrome radar frame (GeoTIFF or ODIM HDF5) is a single-parameter,
 * single-level, single-time gridded field. This reader decodes it into an
 * NFmiQueryData on a native projection area (EPSG:3067 radar data becomes an
 * NFmiTransverseMercatorArea), applying the ODIM-style gain/offset scaling and
 * mapping nodata/undetect to kFloatMissing.
 *
 * The querydata engine converts such frames to a scratch .sqd on arrival and
 * then serves them through the normal memory-mapped querydata path.
 */
// ======================================================================

#pragma once

#include <filesystem>
#include <memory>

class NFmiQueryData;

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
enum class RadarFormat
{
  Auto,       // detect from the filename extension
  GeoTiff,    // .tif / .tiff
  Odim,       // .h5 / .hdf  (not yet implemented)
  QueryData   // .sqd  (no conversion needed)
};

//! Detect the radar file format from the filename extension.
RadarFormat detectRadarFormat(const std::filesystem::path& path);

//! Read a radar GeoTIFF / ODIM HDF5 file into an in-memory NFmiQueryData.
/*!
 * \param path   the source file
 * \param format the format, or RadarFormat::Auto to detect from the extension
 * \return the decoded querydata (never null; throws on failure)
 *
 * Throws Fmi::Exception on any decode error, or if the format is QueryData
 * (those files need no conversion and are loaded directly by the engine).
 */
std::shared_ptr<NFmiQueryData> readRadarFile(const std::filesystem::path& path,
                                             RadarFormat format = RadarFormat::Auto);

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet

// ======================================================================
