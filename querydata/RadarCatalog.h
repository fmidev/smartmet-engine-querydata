// ======================================================================
/*!
 * \brief Cheap per-source catalog of radar frame metadata.
 *
 * For a lazy radar producer the full time dimension must be advertised in
 * GetCapabilities without decoding every frame. This catalog holds, per
 * producer, the header-only metadata (valid/origin time, parameter, grid, CRS,
 * bounding box) of every available frame, obtained via readRadarMetadata() with
 * no pixel decode. It is refreshed from the producer's current file set and is
 * the source of truth for a lazy producer's advertised times; the actual pixel
 * decode happens elsewhere, on access.
 */
// ======================================================================

#pragma once

#include "RadarReader.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class RadarCatalog
{
 public:
  struct Frame
  {
    std::filesystem::path path;
    RadarFrameInfo info;
  };

  // Refresh a producer's catalog from its current file set. Reads header-only
  // metadata for files not already catalogued (or whose mtime changed) and drops
  // entries whose file is gone. A file that fails to parse is skipped. Cheap: no
  // pixel decode. Returns the number of frames now catalogued for the producer.
  std::size_t update(const std::string& producer, const std::vector<std::filesystem::path>& files);

  // All frames of a producer, sorted by valid time (empty if unknown).
  std::vector<Frame> frames(const std::string& producer) const;
  std::size_t frameCount(const std::string& producer) const;

  void remove(const std::string& producer);
  std::vector<std::string> producers() const;

 private:
  struct Entry
  {
    std::filesystem::path path;
    std::filesystem::file_time_type mtime;
    RadarFrameInfo info;
  };

  mutable std::mutex itsMutex;
  std::map<std::string, std::vector<Entry>> itsCatalog;  // producer -> frames
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
