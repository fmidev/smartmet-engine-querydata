// ======================================================================
/*!
 * \brief Standalone test for the size-bounded radar scratch cache.
 *
 * Builds a fake cache tree of per-source subdirectories with controlled sizes
 * and recency (.accessed marker mtimes) and verifies group-LRU eviction order,
 * the low-water target, pinned-source protection, and crash reconciliation
 * (dot-temp / .trash / stale-file removal).
 *
 * Build:  make RadarCacheTest    Run:  ./RadarCacheTest
 */
// ======================================================================

#include "RadarCache.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

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

// Create a source subdir with one .sqd of `bytes`, accessed `ageSeconds` ago.
void makeSource(const fs::path& dir, const std::string& id, std::uintmax_t bytes, int ageSeconds)
{
  const fs::path sub = dir / id;
  fs::create_directories(sub);
  {
    std::ofstream f(sub / (id + "_1.sqd"), std::ios::binary | std::ios::trunc);
    f << std::string(bytes, 'x');
  }
  const fs::path marker = sub / ".accessed";
  {
    std::ofstream f(marker);
  }
  fs::last_write_time(marker, fs::file_time_type::clock::now() - std::chrono::seconds(ageSeconds));
}

std::set<std::string> present(const RadarCache& c)
{
  std::set<std::string> s;
  for (const auto& id : c.sources())
    s.insert(id);
  return s;
}
}  // namespace

int main()
{
  const fs::path root =
      fs::temp_directory_path() /
      ("radarcachetest_" +
       std::to_string(fs::file_time_type::clock::now().time_since_epoch().count()));
  fs::create_directories(root);

  // --- 1. LRU eviction order + low-water target ---
  // 4 sources, 300 B each (total 1200), budget 1000, low-water 0.85 -> target 850.
  // Evict oldest first: 40s (->900), 30s (->600<=850, stop). Keep 20s, 10s.
  for (auto id_age :
       {std::pair<std::string, int>{"src_40", 40}, {"src_30", 30}, {"src_20", 20}, {"src_10", 10}})
    makeSource(root, id_age.first, 300, id_age.second);

  RadarCache cache(root, /*budget=*/1000, /*lowWater=*/0.85);
  check(cache.totalBytes() == 1200, "totalBytes = 1200 before eviction");

  auto neverPinned = [](const std::string&) { return false; };
  cache.enforceBudget(neverPinned);
  auto after = present(cache);
  check(after.count("src_40") == 0, "oldest (src_40) evicted first");
  check(after.count("src_30") == 0, "second-oldest (src_30) evicted to reach low-water");
  check(after.count("src_20") == 1 && after.count("src_10") == 1, "newer sources kept");
  check(cache.totalBytes() == 600, "total at/under low-water after eviction");

  // --- 2. Pinned sources are never evicted ---
  for (auto id_age : {std::pair<std::string, int>{"p_40", 40}, {"p_30", 30}, {"p_20", 20}})
    makeSource(root, id_age.first, 300, id_age.second);
  // Now: src_20, src_10, p_40, p_30, p_20 = 1500 B. Pin the oldest (p_40).
  RadarCache cache2(root, 1000, 0.85);
  auto pinP40 = [](const std::string& id) { return id == "p_40"; };
  cache2.enforceBudget(pinP40);
  auto after2 = present(cache2);
  check(after2.count("p_40") == 1, "pinned oldest source survives eviction");
  check(cache2.totalBytes() <= 1000, "budget met by evicting unpinned sources");

  // --- 3. Reconcile: dot-temp, .trash and stale-file removal ---
  fs::create_directories(root / "recon");
  {
    std::ofstream(root / "recon" / "recon_1.sqd") << std::string(100, 'x');  // keep
    std::ofstream(root / "recon" / "recon_2.sqd") << std::string(100, 'x');  // stale
    std::ofstream(root / "recon" / ".recon_1.sqd.12345") << "partial";       // dot-temp
    std::ofstream(root / "recon" / ".accessed");                             // marker (keep)
  }
  fs::create_directories(root / ".trash" / "leftover");  // interrupted eviction
  std::ofstream(root / ".trash" / "leftover" / "x.sqd") << "junk";
  fs::create_directories(root / "empty_src");  // no data files

  RadarCache cache3(root, 0 /*unlimited*/, 0.85);
  auto keep = [](const std::string& id, const std::string& file)
  { return !(id == "recon" && file == "recon_2.sqd"); };  // mark recon_2 stale
  cache3.reconcile(keep, neverPinned);

  check(!fs::exists(root / ".trash"), "reconcile removed .trash residue");
  check(!fs::exists(root / "recon" / ".recon_1.sqd.12345"), "reconcile removed dot-temp");
  check(fs::exists(root / "recon" / "recon_1.sqd"), "reconcile kept current file");
  check(!fs::exists(root / "recon" / "recon_2.sqd"), "reconcile removed stale file");
  check(fs::exists(root / "recon" / ".accessed"), "reconcile kept the .accessed marker");
  check(!fs::exists(root / "empty_src"), "reconcile removed empty source");

  fs::remove_all(root);

  if (failures == 0)
    std::cout << "RadarCacheTest: ALL PASSED\n";
  else
    std::cout << "RadarCacheTest: " << failures << " FAILURE(S)\n";
  return failures == 0 ? 0 : 1;
}
