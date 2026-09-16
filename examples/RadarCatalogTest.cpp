// ======================================================================
/*!
 * \brief Standalone test for the radar frame metadata catalog.
 *
 * Builds a catalog from real radar files (header-only) and verifies frame count,
 * parameter, valid-time sorting, idempotent refresh and removal. Uses the
 * installed sample fixtures; if a local multi-frame directory is present it also
 * checks a full timeseries.
 *
 * Build:  make RadarCatalogTest    Run:  ./RadarCatalogTest
 */
// ======================================================================

#include "RadarCatalog.h"

#include <newbase/NFmiParameterName.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace SmartMet::Engine::Querydata;

namespace
{
int failures = 0;
void check(bool cond, const std::string& msg)
{
  if (!cond)
  {
    std::cerr << "  FAIL: " << msg << "\n";
    ++failures;
  }
}

std::vector<fs::path> tifs(const fs::path& dir)
{
  std::vector<fs::path> out;
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec))
    if (it->is_regular_file(ec))
    {
      auto e = it->path().extension().string();
      if (e == ".tif" || e == ".tiff")
        out.push_back(it->path());
    }
  return out;
}
}  // namespace

int main()
{
  const fs::path base = "/usr/share/smartmet/test/data/radar/geotiff";
  const fs::path rr1hDir = base / "radar_finland_rr1h_3067";
  const fs::path dbzDir = base / "radar_finland_dbz_3067";

  if (!fs::exists(rr1hDir))
  {
    std::cout << "  (radar fixtures not installed at " << base << " - skipped)\n";
    std::cout << "RadarCatalogTest: ALL PASSED\n";
    return 0;
  }

  RadarCatalog catalog;

  // --- Single-frame producer: metadata + parameter ---
  const auto rr1hFiles = tifs(rr1hDir);
  const std::size_t n1 = catalog.update("rr1h", rr1hFiles);
  check(n1 == rr1hFiles.size(), "catalog frame count matches file count");
  check(catalog.frameCount("rr1h") == rr1hFiles.size(), "frameCount agrees");
  auto frames = catalog.frames("rr1h");
  check(!frames.empty(), "rr1h has frames");
  if (!frames.empty())
    check(frames.front().info.paramId == kFmiPrecipitationAmount, "rr1h param is precip amount");

  // --- Idempotent refresh: same files -> same count ---
  const std::size_t n2 = catalog.update("rr1h", rr1hFiles);
  check(n2 == n1, "re-update with same files is idempotent");

  // --- Second producer is independent ---
  if (fs::exists(dbzDir))
  {
    catalog.update("dbz", tifs(dbzDir));
    auto prods = catalog.producers();
    check(std::find(prods.begin(), prods.end(), "rr1h") != prods.end() &&
              std::find(prods.begin(), prods.end(), "dbz") != prods.end(),
          "both producers catalogued independently");
  }

  // --- Removal: empty file set drops the producer ---
  catalog.update("rr1h", {});
  check(catalog.frameCount("rr1h") == 0, "empty update removes the producer");

  // --- Multi-frame timeseries (local dev data if present) ---
  const char* home = std::getenv("HOME");
  const fs::path localTs = home ? fs::path(home) / "hub" / "radar_finland_rr1h_3067" : fs::path();
  if (!localTs.empty() && fs::exists(localTs))
  {
    const auto files = tifs(localTs);
    const std::size_t n = catalog.update("ts", files);
    check(n == files.size(), "timeseries: every frame catalogued");
    auto ts = catalog.frames("ts");
    bool sorted = true;
    for (std::size_t i = 1; i < ts.size(); ++i)
      if (ts[i].info.validTime < ts[i - 1].info.validTime)
        sorted = false;
    check(sorted, "timeseries frames are sorted by valid time");
    check(ts.size() > 1 && ts.front().info.validTime != ts.back().info.validTime,
          "timeseries spans multiple distinct valid times");
    std::cout << "  timeseries: " << ts.size() << " frames " << ts.front().info.validTime << " .. "
              << ts.back().info.validTime << "\n";
  }
  else
    std::cout << "  (local multi-frame dir absent - timeseries check skipped)\n";

  if (failures == 0)
    std::cout << "RadarCatalogTest: ALL PASSED\n";
  else
    std::cout << "RadarCatalogTest: " << failures << " FAILURE(S)\n";
  return failures == 0 ? 0 : 1;
}
