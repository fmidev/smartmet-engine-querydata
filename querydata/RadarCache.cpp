// ======================================================================
/*!
 * \brief Implementation of the size-bounded radar scratch cache.
 */
// ======================================================================

#include "RadarCache.h"

#include <macgyver/Exception.h>
#include <spine/Convenience.h>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
namespace
{
constexpr const char* kAccessedMarker = ".accessed";
constexpr const char* kTrashDir = ".trash";

// Recency markers are touched at most this often per source.
const std::chrono::seconds kTouchInterval{60};

// Sum sizes of the cached data files (dot-prefixed markers/temps excluded).
std::uintmax_t sourceBytes(const fs::path& subdir, fs::file_time_type& newest)
{
  std::uintmax_t bytes = 0;
  newest = fs::file_time_type::min();
  std::error_code ec;
  for (fs::directory_iterator it(subdir, ec), end; it != end && !ec; it.increment(ec))
  {
    if (!it->is_regular_file(ec))
      continue;
    const std::string name = it->path().filename().string();
    if (!name.empty() && name.front() == '.')
      continue;  // marker or temp
    bytes += it->file_size(ec);
    const auto mt = it->last_write_time(ec);
    if (!ec && mt > newest)
      newest = mt;
  }
  return bytes;
}
}  // namespace

RadarCache::RadarCache(fs::path dir, std::uintmax_t budgetBytes, double lowWaterFraction)
    : itsDir(std::move(dir)),
      itsBudget(budgetBytes),
      itsLowWater(lowWaterFraction > 0.0 && lowWaterFraction <= 1.0 ? lowWaterFraction : 0.85)
{
}

bool RadarCache::isMarker(const std::string& name)
{
  return name == kAccessedMarker || name == ".protected";
}

bool RadarCache::isDotTemp(const std::string& name)
{
  return !name.empty() && name.front() == '.' && !isMarker(name);
}

std::vector<RadarCache::Entry> RadarCache::scan() const
{
  std::vector<Entry> out;
  std::error_code ec;
  if (!fs::exists(itsDir, ec))
    return out;

  for (fs::directory_iterator it(itsDir, ec), end; it != end && !ec; it.increment(ec))
  {
    if (!it->is_directory(ec))
      continue;
    const std::string id = it->path().filename().string();
    if (id == kTrashDir)
      continue;

    fs::file_time_type newest;
    const std::uintmax_t bytes = sourceBytes(it->path(), newest);

    // Recency = the .accessed marker mtime if present, else the newest data file.
    fs::file_time_type atime = newest;
    const fs::path marker = it->path() / kAccessedMarker;
    const auto mt = fs::last_write_time(marker, ec);
    if (!ec)
      atime = mt;

    out.push_back({id, bytes, atime});
  }
  return out;
}

std::uintmax_t RadarCache::totalBytes() const
{
  std::uintmax_t total = 0;
  for (const auto& e : scan())
    total += e.bytes;
  return total;
}

std::vector<std::string> RadarCache::sources() const
{
  std::vector<std::string> ids;
  for (const auto& e : scan())
    ids.push_back(e.id);
  return ids;
}

void RadarCache::markAccessed(const std::string& sourceId) const
{
  std::error_code ec;
  const fs::path subdir = itsDir / sourceId;
  if (!fs::exists(subdir, ec))
    return;
  const fs::path marker = subdir / kAccessedMarker;

  // Rate-limit: skip if touched within the interval.
  const auto now = fs::file_time_type::clock::now();
  const auto last = fs::last_write_time(marker, ec);
  if (!ec && (now - last) < kTouchInterval)
    return;

  // Touch: create if absent, otherwise bump the mtime. A single inode timestamp
  // update is atomic - a crash leaves the old or new time, never garbage.
  if (ec)
  {
    std::ofstream f(marker, std::ios::out | std::ios::trunc);
  }
  else
  {
    fs::last_write_time(marker, now, ec);
  }
}

void RadarCache::evictSource(const std::string& id) const
{
  std::error_code ec;
  const fs::path subdir = itsDir / id;
  if (!fs::exists(subdir, ec))
    return;

  // Atomic commit point: rename the whole source subdir into .trash, then remove.
  // A crash after the rename leaves a .trash entry, reclaimed by reconcile().
  const fs::path trash = itsDir / kTrashDir;
  fs::create_directories(trash, ec);
  const fs::path dest =
      trash /
      (id + "." + std::to_string(fs::file_time_type::clock::now().time_since_epoch().count()));
  fs::rename(subdir, dest, ec);
  if (ec)
  {
    // Fall back to direct removal if rename failed (e.g. different filesystem).
    fs::remove_all(subdir, ec);
    return;
  }
  fs::remove_all(dest, ec);
}

std::uintmax_t RadarCache::enforceBudget(const PinnedFn& pinned) const
{
  if (itsBudget == 0)
    return 0;  // 0 = unlimited

  auto entries = scan();
  std::uintmax_t total = 0;
  for (const auto& e : entries)
    total += e.bytes;
  if (total <= itsBudget)
    return 0;

  const auto lowWater = static_cast<std::uintmax_t>(static_cast<double>(itsBudget) * itsLowWater);

  // Least-recently-accessed first.
  std::sort(entries.begin(),
            entries.end(),
            [](const Entry& a, const Entry& b) { return a.atime < b.atime; });

  std::uintmax_t freed = 0;
  std::size_t skippedPinned = 0;
  for (const auto& e : entries)
  {
    if (total - freed <= lowWater)
      break;
    if (pinned && pinned(e.id))
    {
      ++skippedPinned;
      continue;
    }
    evictSource(e.id);
    freed += e.bytes;
    std::cout << Spine::log_time_str() + " [querydata] radar cache evicted source '" + e.id +
                     "' (" + std::to_string(e.bytes / (1024 * 1024)) + " MB)"
              << '\n';
  }

  if (total - freed > itsBudget)
    std::cout << Spine::log_time_str() +
                     " [querydata] WARNING radar cache still over budget after eviction (" +
                     std::to_string((total - freed) / (1024 * 1024)) + " MB > " +
                     std::to_string(itsBudget / (1024 * 1024)) + " MB); " +
                     std::to_string(skippedPinned) + " source(s) pinned in use"
              << '\n';
  return freed;
}

void RadarCache::reconcile(const KeepFn& keepFile, const PinnedFn& pinned) const
{
  std::error_code ec;
  if (!fs::exists(itsDir, ec))
    return;

  // 1. Remove interrupted-eviction residue.
  fs::remove_all(itsDir / kTrashDir, ec);

  std::size_t droppedTemps = 0, droppedStale = 0, droppedSources = 0;

  for (fs::directory_iterator it(itsDir, ec), end; it != end && !ec; it.increment(ec))
  {
    if (!it->is_directory(ec))
      continue;
    const std::string id = it->path().filename().string();
    if (id == kTrashDir)
      continue;

    std::size_t liveFiles = 0;
    std::vector<fs::path> toRemove;
    std::error_code fec;
    for (fs::directory_iterator fit(it->path(), fec), fend; fit != fend && !fec; fit.increment(fec))
    {
      const std::string name = fit->path().filename().string();
      if (isMarker(name))
        continue;
      if (isDotTemp(name))
      {
        toRemove.push_back(fit->path());  // half-written temp from a crash
        ++droppedTemps;
        continue;
      }
      // A real cached data file: keep only if its source frame is still current.
      if (keepFile && !keepFile(id, name))
      {
        toRemove.push_back(fit->path());
        ++droppedStale;
      }
      else
        ++liveFiles;
    }
    for (const auto& p : toRemove)
      fs::remove(p, ec);

    // A source with no current data files is not really cached; drop it unless pinned.
    if (liveFiles == 0 && !(pinned && pinned(id)))
    {
      fs::remove_all(it->path(), ec);
      ++droppedSources;
    }
  }

  if (droppedTemps || droppedStale || droppedSources)
    std::cout << Spine::log_time_str() + " [querydata] radar cache reconcile: removed " +
                     std::to_string(droppedTemps) + " temp, " + std::to_string(droppedStale) +
                     " stale file(s), " + std::to_string(droppedSources) + " empty source(s)"
              << '\n';

  enforceBudget(pinned);
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
