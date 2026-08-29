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
#include <vector>

#include "xenia/base/cvar.h"
#include "xenia/base/logging.h"

DEFINE_uint32(recent_draw_log_count, 32,
              "Number of recent GPU draws to keep in a ring buffer, dumped "
              "to the log when the GPU is lost. Larger values can help "
              "diagnose hangs that build up over more draws before the "
              "actual failure, at the cost of a larger ring buffer.",
              "GPU.Debug");

namespace xe {

namespace {
struct RecentDrawEntry {
  uint64_t vertex_shader_hash = 0;
  uint64_t pixel_shader_hash = 0;
  uint32_t vertex_count = 0;
  bool indexed = false;
  bool valid = false;
};

// Sized lazily from the cvar on first use, not at static-init time - cvars
// aren't guaranteed to have their final (e.g. command-line-overridden) value
// that early.
std::vector<RecentDrawEntry>& RecentDraws() {
  static std::vector<RecentDrawEntry> draws(
      std::max<uint32_t>(1, cvars::recent_draw_log_count));
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
// unboundedly and is mod-reduced by the (runtime-configurable, not
// necessarily power-of-two) ring size below; at the 2^32 wraparound the slot
// sequence can skip/repeat by a few slots for one cycle - deliberately not
// worth a compare-exchange retry loop here, since that would reintroduce the
// exact hot-path contention this atomic is replacing, to guard against
// something that needs several billion draws to matter even once.
std::atomic<uint32_t> g_recent_draws_next{0};

// RecentDrawLogDump() can be reached from more than one thread for the same
// underlying device-loss event (a GPU backend's submission path and its
// presenter's Present() path can each independently detect it), and it is
// always immediately followed by a fatal exit. Without this guard, two
// threads racing here could have one thread's std::exit() destroying
// process-wide static state while the other thread is still in the middle
// of logging. Only the first caller, ever, does the real work; every other
// caller (whether truly concurrent or just a later, redundant detection of
// the same already-handled loss) returns immediately.
std::atomic<bool> g_recent_draws_dumped{false};
}  // namespace

void RecentDrawLogRecord(uint64_t vertex_shader_hash,
                         uint64_t pixel_shader_hash, uint32_t vertex_count,
                         bool indexed) {
  auto& draws = RecentDraws();
  uint32_t slot =
      g_recent_draws_next.fetch_add(1, std::memory_order_relaxed) %
      static_cast<uint32_t>(draws.size());
  draws[slot] = {vertex_shader_hash, pixel_shader_hash, vertex_count, indexed,
                 true};
}

void RecentDrawLogDump() {
  if (g_recent_draws_dumped.exchange(true, std::memory_order_acq_rel)) {
    return;
  }

  auto& draws = RecentDraws();
  uint32_t count = static_cast<uint32_t>(draws.size());
  uint32_t next = g_recent_draws_next.load(std::memory_order_relaxed);
  XELOGE("Last {} draws issued before the GPU hang (oldest first):", count);
  for (uint32_t i = 0; i < count; ++i) {
    const RecentDrawEntry& entry = draws[(next + i) % count];
    if (!entry.valid) {
      continue;
    }
    XELOGE("  vs=0x{:016X} ps=0x{:016X} {}_count={}", entry.vertex_shader_hash,
          entry.pixel_shader_hash, entry.indexed ? "index" : "vertex",
          entry.vertex_count);
  }
}

}  // namespace xe
