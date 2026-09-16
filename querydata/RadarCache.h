// ======================================================================
/*!
 * \brief Size-bounded, crash-resistant disk cache for decoded radar scratch.
 *
 * The cache holds decoded querydata (.sqd) under one subdirectory per data
 * source (= producer): <dir>/<source-id>/<stem>_<sourcemtime>.sqd. The
 * subdirectory is the unit of admission and eviction, because radar access is
 * source-local: when a source is used its whole timeseries is needed, so a
 * per-file LRU would half-cache hot sources. Eviction therefore drops whole
 * least-recently-accessed sources until the total is under budget.
 *
 * Crash resistance relies on atomicity, not durability (the cache is a pure
 * function of the source files, so anything lost is simply re-decoded):
 *  - the filesystem tree is the only source of truth for what is cached;
 *  - recency is the mtime of a per-source ".accessed" marker (an atomic inode
 *    timestamp update, never torn);
 *  - a whole source is evicted by renaming its subdirectory into <dir>/.trash
 *    (atomic) before removal, so a crash never leaves a half-deleted source;
 *  - reconcile() at startup rebuilds all in-memory state from a tree scan and
 *    removes any dot-prefixed temp, .trash residue or stale file, so it
 *    converges to a consistent state after a crash at any point.
 *
 * All dot-prefixed names (temps and the ".accessed"/".protected" markers) are
 * ignored by the newbase querydata reader, so they never appear as data.
 */
// ======================================================================

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class RadarCache
{
 public:
  // pinned(source-id) -> true if the source must not be evicted (has live models).
  using PinnedFn = std::function<bool(const std::string&)>;
  // keepFile(source-id, filename) -> true if the cached file is still current
  // (its source frame still exists with the same mtime); false marks it stale.
  using KeepFn = std::function<bool(const std::string&, const std::string&)>;

  RadarCache(std::filesystem::path dir, std::uintmax_t budgetBytes, double lowWaterFraction = 0.85);

  // Update a source's recency marker (rate-limited: skipped if touched within
  // itsTouchInterval). Cheap atomic metadata write; safe to call per request.
  void markAccessed(const std::string& sourceId) const;

  // Evict least-recently-accessed whole sources (skipping pinned ones) until the
  // total is at or below the low-water mark. Returns bytes freed. If the budget
  // cannot be met because the unpinned set is exhausted, logs and returns.
  std::uintmax_t enforceBudget(const PinnedFn& pinned) const;

  // Startup reconciliation (replaces a blind wipe): remove .trash residue and
  // dot-prefixed temp leftovers, drop stale files (keepFile == false), drop
  // now-empty unpinned sources, then enforce the budget. Idempotent.
  void reconcile(const KeepFn& keepFile, const PinnedFn& pinned) const;

  std::uintmax_t totalBytes() const;         // sum of cached .sqd sizes
  std::vector<std::string> sources() const;  // source-id (subdir) names present
  const std::filesystem::path& dir() const { return itsDir; }

 private:
  struct Entry
  {
    std::string id;
    std::uintmax_t bytes;
    std::filesystem::file_time_type atime;  // recency (.accessed marker or newest file)
  };

  std::vector<Entry> scan() const;
  void evictSource(const std::string& id) const;

  static bool isMarker(const std::string& name);   // ".accessed" / ".protected"
  static bool isDotTemp(const std::string& name);  // dot-prefixed, not a marker

  std::filesystem::path itsDir;
  std::uintmax_t itsBudget;
  double itsLowWater;
};

}  // namespace Querydata
}  // namespace Engine
}  // namespace SmartMet
