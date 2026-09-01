/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/toast_notification.h"

#include <chrono>
#include <deque>
#include <mutex>

namespace xe {

namespace {
constexpr float kToastVisibleSeconds = 4.0f;
constexpr float kToastFadeSeconds = 0.5f;

std::mutex g_toast_mutex;
std::deque<std::string> g_toast_pending;

bool g_toast_showing = false;
std::string g_toast_current_text;
std::chrono::steady_clock::time_point g_toast_shown_at;
}  // namespace

void ToastNotificationShow(std::string text) {
  std::lock_guard<std::mutex> lock(g_toast_mutex);
  g_toast_pending.push_back(std::move(text));
}

bool ToastNotificationGetCurrent(std::string& out_text, float& out_alpha) {
  std::lock_guard<std::mutex> lock(g_toast_mutex);
  auto now = std::chrono::steady_clock::now();
  if (g_toast_showing) {
    float elapsed =
        std::chrono::duration<float>(now - g_toast_shown_at).count();
    if (elapsed >= kToastVisibleSeconds) {
      g_toast_showing = false;
    } else {
      out_text = g_toast_current_text;
      // Fade in over the first kToastFadeSeconds, fade out over the last.
      float fade_in = elapsed / kToastFadeSeconds;
      float fade_out = (kToastVisibleSeconds - elapsed) / kToastFadeSeconds;
      out_alpha = std::min({1.0f, fade_in, fade_out});
      return true;
    }
  }
  if (!g_toast_pending.empty()) {
    g_toast_current_text = std::move(g_toast_pending.front());
    g_toast_pending.pop_front();
    g_toast_showing = true;
    g_toast_shown_at = now;
    out_text = g_toast_current_text;
    out_alpha = 0.0f;
    return true;
  }
  return false;
}

}  // namespace xe
