<p align="center">
    <a href="https://github.com/xenia-canary/xenia-canary/tree/canary_experimental/assets/icon">
        <img height="256px" src="https://raw.githubusercontent.com/xenia-canary/xenia/master/assets/icon/256.png" />
    </a>
</p>

# Xenia Canary for UWP

An unofficial UWP port of Xenia Canary, targeting Windows 10/11 desktop and
**Xbox One/Series X|S in Developer Mode**. It is not associated with the
Xenia or Xenia Canary developers, or with Microsoft.

This repository is a fork of
[danprice142/xenia-canary-uwp](https://github.com/danprice142/xenia-canary-uwp)
(the original UWP port), rebased onto current
[Xenia Canary](https://github.com/xenia-canary/xenia-canary), which is itself
an experimental fork of [Xenia](https://github.com/xenia-project/xenia). See
[Credits](#credits) below for the full chain.

## What's different in this fork

- Rebased the UWP port onto current Xenia Canary (~440 commits), including
  Xenia's move from Premake to CMake as its build system.
- The UWP app (`xenia-canary-uwp/`) now builds as part of the CMake solution
  via `include_external_msproject()`, instead of a separate, drifted build.
- Fixed the desktop CMake build (`xenia-app`), which had silently stopped
  working: `XE_PLATFORM_WINRT` was hardcoded to `1` for every build, routing
  desktop code through UWP-only paths with no diagnostic. See
  `src/xenia/base/platform.h` and `src/xenia/app/uwp_stubs_win.cc`.
- Added a small always-on ring buffer that logs the last N GPU draws when the
  device is lost, to help narrow down GPU hangs without needing PIX
  (`src/xenia/base/recent_draw_log.h`).
- Various crash fixes in the UWP-specific code: exception handling around the
  file/folder pickers and other `fire_and_forget` coroutines, a stack
  corruption bug in LIVE-signature title scanning, and cross-platform CRT
  fixes (`strncpy_s`/`localtime_s` guarded or replaced with portable
  equivalents) that had crept into shared code.

## Status

This is a hobby port, built and tested by one person against a handful of
titles. Expect rough edges. Desktop (Windows) and UWP (Xbox Dev Mode) Debug
and Release configurations all build and package successfully as of this
fork's latest commit.

## Building

The project uses CMake (see [docs/building.md](docs/building.md) for the
general Xenia build setup). The UWP app additionally requires:

- Visual Studio 2022+ with the **Universal Windows Platform development**
  workload.
- A code-signing certificate matching `Package.appxmanifest`'s
  `Identity Publisher` (generate your own self-signed one for local builds
  and sideloading; see `xenia-canary-uwp/`).

Deploying to an actual Xbox requires
[Developer Mode](https://learn.microsoft.com/en-us/windows/uwp/xbox-apps/devkit-activation)
and the Xbox Device Portal (Apps → Add, uploading both the `.appxbundle` and
its dependencies).

## Game compatibility

This fork tracks the same games as upstream Xenia Canary — see the
[official compatibility tracker](https://github.com/xenia-canary/game-compatibility/issues).

For a searchable/sortable snapshot merging the current Canary tracker with
the older pre-Canary one (1,800+ titles total), with the actual fix pulled
out of each game's discussion thread rather than just a checkbox, see
[`docs/compatibility/xenia-compatibility-ledger.xlsx`](docs/compatibility/xenia-compatibility-ledger-v2.xlsx).
The UWP/Xbox build only has the Direct3D 12 backend available (no native
Vulkan driver on Xbox), so a handful of titles whose fix on desktop requires
switching to Vulkan won't have that specific fix available on Xbox.

## Credits

- [Xenia](https://github.com/xenia-project/xenia) — the original Xbox 360
  emulator project.
- [Xenia Canary](https://github.com/xenia-canary/xenia-canary) — the
  experimental fork this port tracks.
- [danprice142/xenia-canary-uwp](https://github.com/danprice142/xenia-canary-uwp) —
  the original UWP/Xbox port this fork is based on.

## Disclaimer

The goal of this project is to experiment, research, and educate on the topic
of emulation of modern devices and operating systems. **It is not for enabling
illegal activity**. All information is obtained via reverse engineering of
legally purchased devices and games and information made public on the internet
(you'd be surprised what's indexed on Google...).
