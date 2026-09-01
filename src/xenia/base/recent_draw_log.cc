/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/recent_draw_log.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"

DEFINE_uint32(recent_draw_log_count, 32,
              "Number of recent GPU draws to keep in a ring buffer, dumped "
              "to the log when the GPU is lost. Rounded up to the next power "
              "of two. Larger values can help diagnose hangs that build up "
              "over more draws before the actual failure, at the cost of a "
              "larger ring buffer. 0 disables the feature entirely.",
              "GPU.Debug");

namespace xe {

namespace {
struct RecentDrawEntry {
  uint64_t vertex_shader_hash = 0;
  uint64_t pixel_shader_hash = 0;
  uint32_t vertex_count = 0;
  bool indexed = false;
  uint32_t first_vertex_fetch_address = 0;
  bool has_vertex_fetch_address = false;
  uint32_t first_memexport_base_address_dwords = 0;
  bool has_memexport = false;
  bool valid = false;
};

// Sized lazily from the cvar on first use, not at static-init time - cvars
// aren't guaranteed to have their final (e.g. command-line-overridden) value
// that early. Rounded up to a power of two so RecentDrawLogRecord() can mask
// instead of dividing on its hot path; the minimum of 1 here is irrelevant at
// runtime since RecentDrawLogRecord()/RecentDrawLogDump() both skip all work
// when the cvar is 0 (disabled) without ever touching this vector.
std::vector<RecentDrawEntry>& RecentDraws() {
  static std::vector<RecentDrawEntry> draws(
      xe::next_pow2(std::max<uint32_t>(1, cvars::recent_draw_log_count)));
  return draws;
}

// RecentDrawLogRecord() runs on the GPU command thread on every single draw
// call - thousands of times per frame - so slots are claimed with a
// lock-free atomic increment rather than a mutex; only RecentDrawLogDump(),
// which runs at most once, needs the result to actually make sense. The
// entry write itself is a plain (non-atomic) store, so a dump running
// concurrently with a record on the same wrapped-around slot can observe a
// torn entry (fields from two different draws, or `valid` visible before the
// rest) - accepted here since it can only affect the last 1-2 of many log
// lines in a one-time, already-fatal diagnostic dump. This counter grows
// unboundedly and is mask-reduced by the (power-of-two, see RecentDraws())
// ring size below; at the 2^32 wraparound the slot sequence can skip/repeat
// by a few slots for one cycle - deliberately not worth a compare-exchange
// retry loop here, since that would reintroduce the exact hot-path
// contention this atomic is replacing, to guard against something that needs
// several billion draws to matter even once.
std::atomic<uint32_t> g_recent_draws_next{0};

// RecentDrawLogDump() can be reached from more than one thread for the same
// underlying device-loss event (a GPU backend's submission path and its
// presenter's Present() path detect it independently, each through its own
// unrelated one-shot guard before reaching its own call into
// xe::FatalError() -> ShutdownLogging(), which frees the global logger). A
// plain "first caller wins, everyone else returns immediately" flag isn't
// enough here: the *losing* thread must not merely skip its own dump, it
// must not proceed to null/free the logger while the *winning* thread is
// still mid-XELOGE. std::call_once gives exactly that: every other caller
// genuinely blocks until the one execution finishes, rather than racing
// past it, so by the time any caller returns, the dump is guaranteed
// complete. Called an extra time (with no-op result) from
// GraphicsSystem::OnHostGpuLossFromAnyThread and
// Presenter::FatalErrorHostGpuLossCallback -- the two points both detection
// paths funnel through right before their own xe::FatalError() call --
// specifically to enforce that ordering regardless of which path got there
// first.
std::once_flag g_recent_draws_dump_once;
}  // namespace

void RecentDrawLogRecord(uint64_t vertex_shader_hash,
                         uint64_t pixel_shader_hash, uint32_t vertex_count,
                         bool indexed, uint32_t first_vertex_fetch_address,
                         bool has_vertex_fetch_address,
                         uint32_t first_memexport_base_address_dwords,
                         bool has_memexport) {
  if (!cvars::recent_draw_log_count) {
    return;
  }
  auto& draws = RecentDraws();
  uint32_t slot =
      g_recent_draws_next.fetch_add(1, std::memory_order_relaxed) &
      (static_cast<uint32_t>(draws.size()) - 1);
  draws[slot] = {vertex_shader_hash,
                 pixel_shader_hash,
                 vertex_count,
                 indexed,
                 first_vertex_fetch_address,
                 has_vertex_fetch_address,
                 first_memexport_base_address_dwords,
                 has_memexport,
                 true};
}

void RecentDrawLogDump() {
  if (!cvars::recent_draw_log_count) {
    return;
  }
  std::call_once(g_recent_draws_dump_once, [] {
    auto& draws = RecentDraws();
    uint32_t count = static_cast<uint32_t>(draws.size());
    uint32_t next = g_recent_draws_next.load(std::memory_order_relaxed);
    XELOGE("Last {} draws issued before the GPU hang (oldest first):", count);
    for (uint32_t i = 0; i < count; ++i) {
      const RecentDrawEntry& entry = draws[(next + i) & (count - 1)];
      if (!entry.valid) {
        continue;
      }
      std::string suffix;
      if (entry.has_vertex_fetch_address) {
        suffix += fmt::format(" vfetch_addr=0x{:08X}",
                              entry.first_vertex_fetch_address);
      }
      if (entry.has_memexport) {
        suffix += fmt::format(" memexport_addr=0x{:08X}",
                              entry.first_memexport_base_address_dwords << 2);
      }
      XELOGE("  vs=0x{:016X} ps=0x{:016X} {}_count={}{}",
            entry.vertex_shader_hash, entry.pixel_shader_hash,
            entry.indexed ? "index" : "vertex", entry.vertex_count, suffix);
    }
  });
}

}  // namespace xe
