/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_BASE_RECENT_DRAW_LOG_H_
#define XENIA_BASE_RECENT_DRAW_LOG_H_

#include <cstdint>

// Small ring buffer of recently issued draws (shader hashes, vertex/index
// count), for narrowing down GPU hangs. By the time a host GPU API reports
// device removal (potentially long after the actual hang, once the OS TDR
// times out), the command list that caused it has already been reclaimed, so
// backends record draws here as they're issued and dump the tail on loss.
// Lives in xenia-base (rather than a GPU backend library) so both a GPU
// backend and its lower-level presenter/UI counterpart -- which normally
// don't depend on each other -- can both reach it without creating a
// circular library dependency.
namespace xe {

void RecentDrawLogRecord(uint64_t vertex_shader_hash,
                         uint64_t pixel_shader_hash, uint32_t vertex_count,
                         bool indexed);
void RecentDrawLogDump();

}  // namespace xe

#endif  // XENIA_BASE_RECENT_DRAW_LOG_H_
