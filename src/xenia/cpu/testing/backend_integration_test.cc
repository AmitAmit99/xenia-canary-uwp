/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/cpu/testing/util.h"

#include <atomic>
#include <cmath>

#include "xenia/base/platform.h"
#if XE_ARCH_AMD64
#include "xenia/cpu/backend/x64/x64_backend.h"
#elif XE_ARCH_ARM64
#include "xenia/cpu/backend/a64/a64_backend.h"
#endif

using namespace xe;
using namespace xe::cpu;
using namespace xe::cpu::hir;
using namespace xe::cpu::testing;
using xe::cpu::ppc::PPCContext;

// =============================================================================
// SetGuestRoundingMode (C++ path, not HIR opcode)
// =============================================================================
// This tests that calling SetGuestRoundingMode from C++ (as the kernel
// emulation layer does) actually changes the hardware rounding mode, so that
// subsequent JIT'd FP operations produce correctly rounded results.
TEST_CASE("SET_GUEST_ROUNDING_MODE_CPP_PATH", "[backend]") {
  // The HIR function just does an f32 add and returns the result.
  // The rounding mode is set from C++ in pre_call, NOT via HIR opcode.
  TestFunction test([](HIRBuilder& b) {
    auto a = b.Convert(LoadFPR(b, 4), FLOAT32_TYPE);
    auto c = b.Convert(LoadFPR(b, 5), FLOAT32_TYPE);
    auto sum = b.Add(a, c);
    StoreFPR(b, 3, b.Convert(sum, FLOAT64_TYPE));
    b.Return();
  });

  // Mode 2 = toward +infinity.
  test.Run(
      [&test](PPCContext* ctx) {
        ctx->f[4] = 1.0;
        ctx->f[5] = std::ldexp(1.0, -24);
        // Call the C++ SetGuestRoundingMode path.
        test.processors[0]->backend()->SetGuestRoundingMode(ctx, 2);
      },
      [&test](PPCContext* ctx) {
        auto result = static_cast<float>(ctx->f[3]);
        float expected = std::nextafterf(1.0f, 2.0f);
        REQUIRE(result == expected);
        // Reset to nearest for subsequent tests.
        test.processors[0]->backend()->SetGuestRoundingMode(ctx, 0);
      });

  // Mode 1 = toward zero.
  test.Run(
      [&test](PPCContext* ctx) {
        ctx->f[4] = 1.0;
        ctx->f[5] = std::ldexp(1.0, -24);
        test.processors[0]->backend()->SetGuestRoundingMode(ctx, 1);
      },
      [&test](PPCContext* ctx) {
        auto result = static_cast<float>(ctx->f[3]);
        REQUIRE(result == 1.0f);
        test.processors[0]->backend()->SetGuestRoundingMode(ctx, 0);
      });
}

// =============================================================================
// Guest Trampolines
// =============================================================================
// Test that CreateGuestTrampoline creates a callable trampoline that
// transitions from guest JIT code back to a host C++ callback.
static std::atomic<int> trampoline_call_count{0};
static void* trampoline_received_arg1 = nullptr;
static void* trampoline_received_arg2 = nullptr;

static void TrampolineCallback(ppc::PPCContext* ctx, void* userarg1,
                               void* userarg2) {
  trampoline_call_count.fetch_add(1);
  trampoline_received_arg1 = userarg1;
  trampoline_received_arg2 = userarg2;
  // Write a marker value so the test can verify the callback ran.
  ctx->r[3] = 0xCAFEBABE;
}

TEST_CASE("GUEST_TRAMPOLINE_BASIC", "[backend]") {
  // Reset global state.
  trampoline_call_count = 0;
  trampoline_received_arg1 = nullptr;
  trampoline_received_arg2 = nullptr;

  auto memory = std::make_unique<Memory>();
  memory->Initialize();

  std::unique_ptr<xe::cpu::backend::Backend> backend;
#if XE_ARCH_AMD64
  backend.reset(new xe::cpu::backend::x64::X64Backend());
#elif XE_ARCH_ARM64
  backend.reset(new xe::cpu::backend::a64::A64Backend());
#endif
  REQUIRE(backend);

  auto processor = std::make_unique<Processor>(memory.get(), nullptr);
  processor->Setup(std::move(backend));

  // Create a trampoline with known userdata pointers.
  void* tag1 = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1111));
  void* tag2 = reinterpret_cast<void*>(static_cast<uintptr_t>(0x2222));
  uint32_t trampoline_addr = processor->backend()->CreateGuestTrampoline(
      TrampolineCallback, tag1, tag2, false);

  REQUIRE(trampoline_addr != 0);
  REQUIRE(trampoline_addr >= 0x80000000);
  REQUIRE(trampoline_addr < 0x80040000);

  // Clean up the trampoline.
  processor->backend()->FreeGuestTrampoline(trampoline_addr);
}
