/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_BASE_TOAST_NOTIFICATION_H_
#define XENIA_BASE_TOAST_NOTIFICATION_H_

#include <string>

// Small cross-thread toast notification queue (e.g. "Achievement unlocked:
// ..."). Lives in xenia-base so a kernel module (which enqueues, from a
// guest thread, with no UI dependency) and the app's UI layer (which
// dequeues and draws, from the UI thread) can both reach it without the
// kernel depending on ImGui/the UI library.
namespace xe {

// Thread-safe; queues the text for display. May be called from any thread.
void ToastNotificationShow(std::string text);

// UI-thread only, call once per frame. If a toast should currently be
// visible, fills out_text and out_alpha (0-1, for fade in/out) and returns
// true; otherwise returns false and leaves the outputs untouched.
bool ToastNotificationGetCurrent(std::string& out_text, float& out_alpha);

}  // namespace xe

#endif  // XENIA_BASE_TOAST_NOTIFICATION_H_
