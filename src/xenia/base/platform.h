/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_BASE_PLATFORM_H_
#define XENIA_BASE_PLATFORM_H_

// This file contains the main platform switches used by xenia as well as any
// fixups required to normalize the environment. Everything in here should be
// largely portable.
// Platform-specific headers, like platform_win.h, are used to house any
// super platform-specific stuff that implies code is not platform-agnostic.
//
// NOTE: ordering matters here as sometimes multiple flags are defined on
// certain platforms.
//
// Great resource on predefined macros:
// https://sourceforge.net/p/predef/wiki/OperatingSystems/
// Original link: https://predef.sourceforge.net/preos.html

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(TARGET_OS_MAC) && TARGET_OS_MAC
#define XE_PLATFORM_MAC 1
#elif defined(WIN32) || defined(_WIN32)
#define XE_PLATFORM_WIN32 1
// WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) is false only in an
// actual UWP/Windows Store app-container build (ApplicationType=Windows
// Store), where desktop Win32 APIs aren't available - a signal the MSVC
// toolchain sets automatically per translation unit, unlike a hand-written
// CMake define that only reaches the one target it's added to. winapifamily.h
// is a standalone header safe to include this early, before <windows.h>.
// This used to be hardcoded to 1 unconditionally (see the removed TO-DO),
// which routed every non-UWP build - including this CMake desktop target -
// through UWP-only code paths (UWP:: stub calls, invalid UWPWindow casts on
// real Win32 windows, wrong storage-folder resolution) with no diagnostic.
//
// KNOWN RESIDUAL LIMITATION: this correctly reflects the compiling project's
// own ApplicationType for code the *build system* compiles once per final
// consumer (xenia-app.vcxproj, xenia-canary-uwp.vcxproj's own small
// ClCompile list). It cannot be correct for xenia-base/xenia-ui/etc: per
// xenia-canary-uwp/CMakeLists.txt, xenia-canary-uwp.vcxproj links those
// as prebuilt .lib output from the desktop CMake target (by design -
// AppContainer packaging isn't expressible in CMake's target model) rather
// than recompiling them itself, so those shared sources are compiled
// exactly once, under desktop settings, and that same object code ends up
// linked into the real Xbox/UWP binary too - meaning any XE_PLATFORM_WINRT-
// gated behavior inside them (GetUserFolder()'s UWP::GetLocalState() vs.
// SHGetKnownFolderPath in filesystem_win.cc, the on-screen keyboard
// invocations in create_profile_ui.cc/signin_ui.cc, xinput_input_driver.cc's
// UWPWindow calls) resolves to the desktop answer even on real hardware.
// Fixing this for real means either giving xenia-canary-uwp.vcxproj its own
// compile of those sources under ApplicationType=Windows Store, or a second
// CMake configuration dedicated to producing UWP-flavored versions of them -
// both a build-system change well beyond this macro, and not something
// verifiable without deploying to an actual Xbox devkit.
#include <winapifamily.h>
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define XE_PLATFORM_WINRT 1
#endif
#elif defined(__ANDROID__)
#define XE_PLATFORM_ANDROID 1
#define XE_PLATFORM_LINUX 1
#elif defined(__gnu_linux__)
#define XE_PLATFORM_GNU_LINUX 1
#define XE_PLATFORM_LINUX 1
#else
#error Unsupported target OS.
#endif

#if defined(__clang__) && !defined(_MSC_VER)  // chrispy: support clang-cl
#define XE_COMPILER_CLANG 1
#define XE_COMPILER_HAS_CLANG_EXTENSIONS 1
#elif defined(__GNUC__)
#define XE_COMPILER_GNUC 1
#define XE_COMPILER_HAS_GNU_EXTENSIONS 1
#elif defined(_MSC_VER)
#define XE_COMPILER_MSVC 1
#define XE_COMPILER_HAS_MSVC_EXTENSIONS 1
#elif defined(__MINGW32)
#define XE_COMPILER_MINGW32 1
#define XE_COMPILER_HAS_GNU_EXTENSIONS 1
#elif defined(__INTEL_COMPILER)
#define XE_COMPILER_INTEL 1
#else
#define XE_COMPILER_UNKNOWN 1
#endif
// chrispy: had to place this here.
#if defined(__clang__) && defined(_MSC_VER)
#define XE_COMPILER_CLANG_CL 1
#define XE_COMPILER_HAS_CLANG_EXTENSIONS 1
#endif

// clang extensions == superset of gnu extensions
#if XE_COMPILER_HAS_CLANG_EXTENSIONS == 1
#define XE_COMPILER_HAS_GNU_EXTENSIONS 1
#endif

#if defined(_M_AMD64) || defined(__amd64__)
#define XE_ARCH_AMD64 1
#elif defined(_M_ARM64) || defined(__aarch64__)
#define XE_ARCH_ARM64 1
#elif defined(_M_IX86) || defined(__i386__) || defined(_M_ARM) || \
    defined(__arm__)
#error Xenia is not supported on 32-bit platforms.
#elif defined(_M_PPC) || defined(__powerpc__)
#define XE_ARCH_PPC 1
#endif

#if XE_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX  // Don't want windows.h including min/max macros.
#endif            // XE_PLATFORM_WIN32

#if XE_PLATFORM_WIN32
#include <intrin.h>
#elif XE_ARCH_AMD64
#include <x86intrin.h>
#endif  // XE_PLATFORM_WIN32

#if XE_PLATFORM_MAC
#include <libkern/OSByteOrder.h>
#endif  // XE_PLATFORM_MAC

#if XE_COMPILER_MSVC
#define _XEPACKEDSCOPE(body) __pragma(pack(push, 1)) body __pragma(pack(pop));
#else
#define _XEPACKEDSCOPE(body)     \
  _Pragma("pack(push, 1)") body; \
  _Pragma("pack(pop)");
#endif  // XE_PLATFORM_WIN32

#define XEPACKEDSTRUCT(name, value) _XEPACKEDSCOPE(struct name value)
#define XEPACKEDSTRUCTANONYMOUS(value) _XEPACKEDSCOPE(struct value)
#define XEPACKEDUNION(name, value) _XEPACKEDSCOPE(union name value)

#if XE_COMPILER_HAS_MSVC_EXTENSIONS == 1
#define XE_FORCEINLINE __forceinline
#define XE_NOINLINE __declspec(noinline)
// can't properly emulate "cold" in msvc, but can still segregate the function
// into its own seg
#define XE_COLD __declspec(code_seg(".cold"))
#define XE_LIKELY(...) (!!(__VA_ARGS__))
#define XE_UNLIKELY(...) (!!(__VA_ARGS__))
#define XE_MSVC_ASSUME(...) __assume(__VA_ARGS__)
#define XE_NOALIAS __declspec(noalias)
#elif XE_COMPILER_HAS_GNU_EXTENSIONS == 1
#define XE_FORCEINLINE __attribute__((always_inline))
#define XE_NOINLINE __attribute__((noinline))
#define XE_COLD __attribute__((cold))
#define XE_LIKELY(...) __builtin_expect(!!(__VA_ARGS__), true)
#define XE_UNLIKELY(...) __builtin_expect(!!(__VA_ARGS__), false)
#define XE_NOALIAS
// cant do unevaluated assume
#define XE_MSVC_ASSUME(...) static_cast<void>(0)
#else
#define XE_FORCEINLINE inline
#define XE_NOINLINE
#define XE_COLD

#define XE_LIKELY_IF(...) if (!!(__VA_ARGS__)) [[likely]]
#define XE_UNLIKELY_IF(...) if (!!(__VA_ARGS__)) [[unlikely]]
#define XE_NOALIAS
#define XE_MSVC_ASSUME(...) static_cast<void>(0)

#endif
#if XE_COMPILER_HAS_MSVC_EXTENSIONS == 1
#define XE_MSVC_OPTIMIZE_SMALL() __pragma(optimize("s", on))
#define XE_MSVC_OPTIMIZE_REVERT() __pragma(optimize("", on))
#else
#define XE_MSVC_OPTIMIZE_SMALL()
#define XE_MSVC_OPTIMIZE_REVERT()
#endif

#if XE_COMPILER_HAS_GNU_EXTENSIONS == 1
#define XE_LIKELY_IF(...) if (XE_LIKELY(__VA_ARGS__))
#define XE_UNLIKELY_IF(...) if (XE_UNLIKELY(__VA_ARGS__))
#define XE_MAYBE_UNUSED __attribute__((unused))
#else
#define XE_LIKELY_IF(...) if (!!(__VA_ARGS__)) [[likely]]
#define XE_UNLIKELY_IF(...) if (!!(__VA_ARGS__)) [[unlikely]]
#define XE_MAYBE_UNUSED
#endif
// only use __restrict if MSVC, for clang/gcc we can use -fstrict-aliasing which
// acts as __restrict across the board todo: __restrict is part of the type
// system, we might actually have to still emit it on clang and gcc
#if XE_COMPILER_CLANG_CL == 0 && XE_COMPILER_MSVC == 1

#define XE_RESTRICT __restrict
#else
#define XE_RESTRICT
#endif

#if XE_ARCH_AMD64 == 1
#define XE_HOST_CACHE_LINE_SIZE 64
#elif XE_ARCH_ARM64 == 1
#define XE_HOST_CACHE_LINE_SIZE 64
#else

#error unknown cache line size for unknown architecture!
#endif

namespace xe {

#if XE_PLATFORM_WIN32
constexpr char kPathSeparator = '\\';
#else
constexpr char kPathSeparator = '/';
#endif  // XE_PLATFORM_WIN32

constexpr char kGuestPathSeparator = '\\';

}  // namespace xe
#if XE_ARCH_AMD64 == 1
#include "platform_amd64.h"
#elif XE_ARCH_ARM64 == 1
#include "platform_arm64.h"
#endif
#endif  // XENIA_BASE_PLATFORM_H_
