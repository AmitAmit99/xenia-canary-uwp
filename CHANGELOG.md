# Changelog

All notable changes to this fork of `xenia-canary-uwp`, on top of updating it to current Xenia Canary.
Per-version detail is below; this section is the full rollup against the
original [danprice142/xenia-canary-uwp](https://github.com/danprice142/xenia-canary-uwp).

## Everything different from the original xenia-canary-uwp

### Rebased onto current Xenia Canary
- Updated the UWP port onto current Xenia Canary (~440 commits ahead of where
  the original was forked), including Xenia's move from Premake to CMake.
- The UWP app now builds as part of the CMake solution
  (`include_external_msproject()`) instead of a separate, drifted build setup.
- Fixed the desktop CMake build (`xenia-app`), which had silently stopped
  working: `XE_PLATFORM_WINRT` was hardcoded to `1` for every build, routing
  desktop code through UWP-only paths. Kept it hardcoded (real hardware needs
  that - see `src/xenia/base/platform.h`) but added a narrow, per-file
  override (`XE_APP_DESKTOP_BUILD`) so the desktop build behaves like a
  desktop build again, without touching shared-library behavior.

### New features (didn't exist in the original at all)
- **In-game pause menu**: hold View, press Menu, at any time - including
  mid-gameplay - to bring up an overlay with Resume, Exit Game, quick
  settings, and a recently-played list.
- **Achievement toast notifications**: on-screen "Achievement Unlocked"
  popups during actual gameplay, not just in a hidden list screen.
- **Recently Played quick-switch**: jump straight to a different recent title
  from the pause menu - the only way to switch games mid-session on this
  port, since there's no in-process return to the dashboard.
- **In-pause-menu quick settings**: Mute Audio / Controller Vibration toggles
  without exiting the game.
- **GPU backend selector** in Settings, plus two more exposed compatibility
  cvars (`disable_context_promotion`, `gpu_allow_invalid_upload_range`) that
  desktop Xenia Canary already had but this port didn't expose.
- **GPU-hang diagnostic**: a ring buffer logging the last N draws (shader
  hashes, vertex counts, vertex-fetch/memexport addresses) dumped on device
  loss, plus accurate hang-vs-external-loss classification via
  `GetDeviceRemovedReason()` - built to debug real GPU hangs (e.g. SVR11)
  without needing PIX.
- **"Apply Optimized Settings" manual-browse fallback** when no community
  config exists online for a title, instead of a dead-end message.

### Fixed (bugs present in the original)
- Game list showed duplicate entries for titles with both `default.xex` and a
  signed companion file (e.g. `.nxeart`).
- "Apply Optimized Settings" fetched the wrong file format and silently
  failed for every title.
- LB/RB blade-switching could crash back to the dashboard if pressed while a
  dialog/popup/editor was open.
- A data-loss bug (introduced and caught within this fork's own work, but
  worth noting): a malformed settings-download response could silently
  overwrite an existing per-game config with garbage before any validation.
- Various crash fixes in the UWP-specific code: exception handling around
  file/folder pickers and other `fire_and_forget` coroutines, a stack
  corruption bug in LIVE-signature title scanning, and cross-platform CRT
  fixes (`strncpy_s`/`localtime_s`) that had crept into shared code.
- A thread-safety race and a missing diagnostic dump on one GPU-loss path.

### Project
- Established as a real GitHub fork (`gh repo fork`) with its own releases,
  rather than a standalone copy.
- Compatibility ledger expanded from 1083 to 1830 titles, merging in a legacy
  pre-Canary tracker with real extracted fix notes instead of a bare "works"
  marker.

### Investigated, not fixed
- SVR11 (WWE SmackDown vs. Raw 2011) hangs on D3D12: root-caused to ~148,000
  single-vertex memexport draws overwhelming D3D12's per-draw submission
  overhead. The only known fix is Vulkan, which doesn't exist on Xbox at all
  (no driver, not just a missing flag - confirmed by linking it in and
  testing, then reverting). No safe code-level fix identified yet.

## 1.1.7.13

### Added
- **Achievement toast notifications**: unlocking an achievement now shows an on-screen "Achievement Unlocked: <name> (<gamerscore>G)" toast during gameplay, not just in the (never-shown-mid-game) achievements list screen. New `xenia-base` module (`toast_notification.h/.cc`) so the kernel-side unlock code (`user_tracker.cc`) can enqueue a toast without depending on the UI library; drawn every frame by the pause-menu dialog regardless of whether the menu itself is open.
- **Recently Played, in the pause menu**: up to 5 recently launched titles are now listed in the View+Menu pause overlay, one tap to switch straight to a different game. This is also the only way to switch games mid-session on this port — there's no in-process "return to dashboard" from a running title.
- **In-pause-menu quick settings**: Mute Audio and Controller Vibration toggles, directly in the View+Menu overlay — no need to exit the game to change them.

## 1.1.7.12

### Added
- **In-game pause menu**: hold View and press Menu (on the controller) at any time — including mid-gameplay, not just in the dashboard — to bring up a "Resume" / "Exit Game" overlay. Press the chord again or B to dismiss.
  - New persistent dialog (`EmulatorWindow::PauseMenuDialog`) that, unlike the frontend dashboard dialog, is never destroyed once a title launches, so it keeps checking the button chord every frame regardless of game state.
  - New `UWP::ExitApplication()` (desktop stub: process exit) since there's no in-process "return to the Xenia dashboard" from a running title on this port — exiting the game means exiting the app, same as leaving any other Xbox title.

## 1.1.7.11

### Changed
- Reverted an experimental attempt to enable the Vulkan graphics backend on Xbox. Confirmed Xbox Developer Mode has no Vulkan driver at all (D3D12 only, at the OS/hardware level), so this can't work regardless of what Xenia links — reverted cleanly rather than shipping dead weight.

## 1.1.7.8 – 1.1.7.9

### Fixed
- **Data-loss regression**: downloading "Apply Optimized Settings" for a title with a malformed server response (rate-limit page, HTML error, truncated read) could silently overwrite an existing per-game config with garbage, with no validation before the write. Restored a validation gate (TOML parse check) before writing to disk.
- **Dead code removed**: `ConvertOptimizedConfigJsonToToml` and its `EscapeTomlString` helper had zero remaining callers after the settings-download format fix — deleted (declaration, implementation, and desktop stub).
- **Guard inconsistency**: the blade-switching input guard (`blade_switch_blocked`) and the pre-existing game-list input guard (`is_any_game_context_open`) were supposed to represent the same condition but had drifted — `show_action_status_` was included in one but not the other. Synced them.
- "Apply Optimized Settings" now offers a manual-browse fallback when no community config exists online for a title, instead of dead-ending on an info message (matches the existing "Apply Patch" behavior).

### Added
- GPU-hang diagnostic: `RecentDrawLogRecord`/`RecentDrawLogDump` — a ring buffer that records the last N draw calls (shader hashes, vertex counts, vertex-fetch addresses, memexport addresses) and dumps them to the log the moment the GPU is lost, to help diagnose hangs like SVR11's.
- Accurate device-loss diagnosis: checks `GetDeviceRemovedReason()` to distinguish a genuine GPU hang from an externally-caused device loss instead of guessing.

### Investigated, not fixed
- SVR11 (WWE SmackDown vs. Raw 2011) hangs on D3D12: root-caused to ~148,000 single-vertex memexport draws issued in a tight loop, overwhelming D3D12's per-draw submission overhead within Windows' TDR window. The only known working fix is switching to the Vulkan backend, which doesn't exist on Xbox (see 1.1.7.11). No safe code-level fix identified yet — draw-batching was ruled out because most of these draws don't read from a walkable vertex buffer at all (memexport-only, no sequential vertex-fetch address to merge on).

## 1.1.7.7 and earlier

### Fixed
- LB/RB blade-switching could crash back to the dashboard if pressed while a dialog/popup/context menu/editor was open — added a guard (other button handlers already had one; this one didn't).
- Game list showed duplicate entries for titles with both `default.xex` and a signed companion file (e.g. `.nxeart`) — now only `default.xex` shows.
- "Apply Optimized Settings" downloaded the wrong file format (fetched `.json` from a repo that only serves `.toml`) — every download silently failed.
- A platform-detection regression (deriving `XE_PLATFORM_WINRT` from `WINAPI_FAMILY_PARTITION` instead of keeping it hardcoded) that would have silently broken real Xbox hardware behavior (`GetUserFolder`, xinput, on-screen keyboard) — caught and reverted before shipping.
- A thread-safety race in the GPU-hang diagnostic dump, and a missing dump on the `DXGI_ERROR_DEVICE_RESET` code path.

### Added
- GPU backend selector in Settings (Any/D3D12, plus Vulkan/Null on non-Xbox desktop builds), and two more exposed cvars: `disable_context_promotion`, `gpu_allow_invalid_upload_range`.
- Per-game config editor, "Apply Config Overrides" (browse and import any `.toml`), and "Apply Optimized Settings" (auto-download from the community repo) — matching desktop Xenia Canary's settings capabilities.

### Project
- Established as a real GitHub fork of `danprice142/xenia-canary-uwp` (not just a copy).
- Compatibility ledger expanded from 1083 to 1830 titles, merging in a legacy pre-Canary tracker with real extracted fix notes instead of a bare "works" marker.
- Updated to current Xenia Canary upstream (`canary_experimental` merge).
