/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

// XE_PLATFORM_WINRT is hardcoded to 1 in xenia/base/platform.h for every
// build (see the comment there for why: xenia-base/xenia-kernel/xenia-ui are
// compiled exactly once, by ordinary desktop CMake, and that same object
// code is linked directly into the real Xbox/UWP binary, so there is no
// build-wide signal that means "this compile is for the UWP app" without
// breaking that shared code on real hardware). That means shared/desktop
// code still calls into the UWP:: namespace unconditionally under
// `#if XE_PLATFORM_WINRT` guards even in this desktop build (e.g.
// imgui_drawer.cc/presenter.cc's SetUIOpen calls), so the namespace needs
// *some* definition for xenia-app to link against. The real UWP::
// definitions live in xenia-canary-uwp/*.cpp, which are only compiled by the
// hand maintained xenia-canary-uwp.vcxproj, not by CMake. This file supplies
// minimal desktop stand-ins so xenia-app can link. It is named with the
// "_win" suffix so xe_platform_sources() only compiles it into the desktop
// xenia-app CMake target on Windows; it is never compiled by the
// xenia-canary-uwp.vcxproj, so it does not affect the Xbox build.

// platform.h sets WIN32_LEAN_AND_MEAN/NOMINMAX before anything else in this
// TU can drag in <windows.h> -- otherwise windows.h's min/max macros corrupt
// every std::min/std::max call in headers included afterward.
#include "xenia/base/platform.h"

// This file is a desktop-only stand-in for the real UWP:: definitions in
// xenia-canary-uwp/*.cpp (see the file-level comment above) and must never be
// compiled alongside them -- doing so would double-define g_char_buffer/
// g_buffer_mutex below (an ODR violation) and the UWPWindow methods further
// down. The build-system convention (xe_platform_sources() only pulling this
// "_win" file into the desktop CMake target) is what actually keeps that from
// happening; this is a compile-time trip-wire in case that ever changes.
// Checks XE_APP_DESKTOP_BUILD (defined only by src/xenia/app/CMakeLists.txt
// for this exact target), not XE_PLATFORM_WINRT -- the latter is hardcoded
// to 1 everywhere (see above) and so can't distinguish this build from a
// real UWP one.
#ifndef XE_APP_DESKTOP_BUILD
#error \
    "uwp_stubs_win.cc must only be compiled into the desktop xenia-app CMake target -- it duplicates definitions that live in xenia-canary-uwp/*.cpp."
#endif  // !XE_APP_DESKTOP_BUILD

#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

#include "xenia-canary-uwp/UWPUtil.h"
#include "xenia-canary-uwp/WinRTKeyboard.h"
#include "xenia-canary-uwp/XeniaUWP.h"
#include "xenia-canary-uwp/window_uwp.h"
#include "xenia/base/filesystem.h"
#include "xenia/base/string.h"

// xenia-hid-xinput's xinput_input_driver.cc casts window() to UWPWindow* and
// calls these (also only defined in xenia-canary-uwp/*.cpp, which the
// xenia-app CMake target never compiles) under the same always-true
// XE_PLATFORM_WINRT guard. xenia-app never actually constructs a UWPWindow,
// so these bodies just need to exist for the link, not do anything real.
namespace xe {
namespace ui {
void UWPWindow::SetXInputDriver(xe::hid::xinput::XInputInputDriver* driver) {
  input_driver = driver;
}
void UWPWindow::ClearXInputDriver() { input_driver = nullptr; }
}  // namespace ui
}  // namespace xe

namespace UWP {

std::vector<uint32_t> g_char_buffer;
std::mutex g_buffer_mutex;

void SelectGameFromWinRT(xe::Emulator* emu) {}
bool HasGamePath() { return false; }
void SelectFolder(std::function<void(std::string)> callback) {}
void SelectFile(std::function<void(std::string)> callback) {}
void SelectFiles(std::function<void(std::vector<std::string>)> callback) {}
bool TestPathPermissions(std::string path) { return true; }

std::string GetLocalCache() {
  // Deliberately xe::filesystem::GetExecutablePath() (unconditional, no
  // XE_PLATFORM_WINRT branch), not GetExecutableFolder(): since
  // XE_PLATFORM_WINRT is hardcoded to 1 for every build (see platform.h),
  // GetExecutableFolder() itself calls UWP::GetLocalState() -> this
  // function on this desktop build, which would recurse into itself.
  return xe::to_utf8(
      xe::filesystem::GetExecutablePath().parent_path().u16string());
}

std::string GetLocalState() { return GetLocalCache(); }

int GetCoreDPI() { return 96; }
void SetAutomaticLaunch(std::string game_path) {}
void SetDPI(int DPI) {}
bool IsUIOpen() { return false; }
void SetUIOpen(bool is_open) {}
void LaunchUri(const std::string& url) {}

namespace {
constexpr char kNotSupportedInDesktopBuild[] = "Not supported in the desktop build.";

void RejectNotSupported(const std::function<void(bool, std::string)>& callback) {
  if (callback) callback(false, kNotSupportedInDesktopBuild);
}
}  // namespace

void DownloadAndExtractZip(const std::string& url,
                           const std::string& dest_folder,
                           std::function<void(bool, std::string)> callback) {
  RejectNotSupported(callback);
}
bool IsDownloadInProgress() { return false; }
float GetDownloadProgress() { return 0.0f; }

std::string GetTitleIdFromPath(const std::string& game_path) { return ""; }

void DownloadPatchesForGame(const std::string& title_id,
                            const std::string& dest_folder,
                            std::function<void(bool, std::string)> callback) {
  RejectNotSupported(callback);
}
void DownloadPluginsForGame(const std::string& title_id,
                            const std::string& dest_folder,
                            std::function<void(bool, std::string)> callback) {
  RejectNotSupported(callback);
}
void DownloadConfigForGame(const std::string& title_id,
                           const std::string& dest_folder,
                           std::function<void(bool, std::string)> callback) {
  RejectNotSupported(callback);
}
bool ConvertOptimizedConfigJsonToToml(const std::string& json,
                                      std::string& out_toml) {
  return false;
}
std::string GetMediaIdFromPath(const std::string& game_path) { return ""; }
void EnsureUnityMetadataFetch(const std::string& title_id) {}
bool TryGetUnityMetadata(const std::string& title_id,
                         UnityGameMetadata* out_metadata) {
  return false;
}
void DownloadTitleUpdatesForGame(
    const std::string& title_id, const std::string& media_id,
    const std::string& dest_folder,
    std::function<void(bool, std::vector<std::string>, std::string)>
        callback) {
  if (callback) callback(false, {}, kNotSupportedInDesktopBuild);
}

void StartXenia() {}
void ExecutePendingFunctionsFromUIThread() {}
void RegisterXeniaWindow(xe::ui::Window* window) {}
void UpdateImGuiIO() {}
void RefreshPaths() {}
std::vector<std::tuple<std::string, std::string>> GetGames() { return {}; }
std::vector<std::string> GetPaths() { return {}; }
void SetGamePaths(std::vector<std::string> paths) {}

void ShowKeyboard() {}
void HandleCharacter(uint32_t keycode) {}

}  // namespace UWP
