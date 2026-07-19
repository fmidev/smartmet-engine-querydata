// ======================================================================
/*!
 * \brief Implementation of the radar frame metadata catalog.
 */
// ======================================================================

#include "RadarCatalog.h"

#include <macgyver/Exception.h>
#include <spine/Convenience.h>

#include <algorithm>
#include <iostream>
#include <set>

namespace fs = std::filesystem;

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
std::size_t RadarCatalog::update(const std::string& producer, const std::vector<fs::path>& files)
{
  // Read metadata OUTSIDE the lock (it opens files); only the map update is
  // locked. Build the new frame set, reusing existing entries whose file+mtime
  // are unchanged so a rescan does not re-read the header of every frame.
  std::vector<Entry> existing;
  {
    std::lock_guard<std::mutex> lock(itsMutex);
    auto it = itsCatalog.find(producer);
    if (it != itsCatalog.end())
      existing = it->second;
  }

  auto findExisting = [&](const fs::path& p, fs::file_time_type mt) -> const Entry*
  {
    for (const auto& e : existing)
      if (e.path == p && e.mtime == mt)
        return &e;
    return nullptr;
  };

  std::vector<Entry> fresh;
  fresh.reserve(files.size());
  for (const auto& f : files)
  {
    std::error_code ec;
    const auto mt = fs::last_write_time(f, ec);
    if (ec)
      continue;  // file vanished between scan and here
    if (const Entry* keep = findExisting(f, mt))
    {
      fresh.push_back(*keep);
      continue;
    }
    try
    {
      Entry e;
      e.path = f;
      e.mtime = mt;
      e.info = readRadarMetadata(f);
      fresh.push_back(std::move(e));
    }
    catch (...)
    {
      // A frame that cannot be read header-only is skipped, not fatal.
      std::cout << Spine::log_time_str() +
                       " [querydata] radar catalog: skipping unreadable frame " + f.string()
                << '\n';
    }
  }

  std::sort(fresh.begin(),
            fresh.end(),
            [](const Entry& a, const Entry& b) { return a.info.validTime < b.info.validTime; });

  const std::size_t count = fresh.size();
  {
    std::lock_guard<std::mutex> lock(itsMutex);
    if (fresh.empty())
      itsCatalog.erase(producer);
    else
      itsCatalog[producer] = std::move(fresh);
  }
  return count;
}

std::vector<RadarCatalog::Frame> RadarCatalog::frames(const std::string& producer) const
{
  std::lock_guard<std::mutex> lock(itsMutex);
  std::vector<Frame> out;
  auto it = itsCatalog.find(producer);
  if (it != itsCatalog.end())
  {
    out.reserve(it->second.size());
    for (const auto& e : it->second)
      out.push_back({e.path, e.info});
  }
  return out;
}

std::size_t RadarCatalog::frameCount(const std::string& producer) const
{
  std::lock_guard<std::mutex> lock(itsMutex);
  auto it = itsCatalog.find(producer);
  return it != itsCatalog.end() ? it->second.size() : 0;
}

void RadarCatalog::remove(const std::string& producer)
{
  std::lock_guard<std::mutex> lock(itsMutex);
  itsCatalog.erase(producer);
}

std::vector<std::string> RadarCatalog::producers() const
{
  std::lock_guard<std::mutex> lock(itsMutex);
  std::vector<std::string> out;
  out.reserve(itsCatalog.size());
  for (const auto& kv : itsCatalog)
    out.push_back(kv.first);
  return out;
}

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
