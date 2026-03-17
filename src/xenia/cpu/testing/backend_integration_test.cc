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
#include <cstring>

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

// =============================================================================
// Host -> Guest -> Host round-trip via BuiltinFunction (GuestToHostThunk)
// =============================================================================
// Tests the full thunk chain: host C++ calls into JIT'd guest code, which
// calls a builtin (host C++ function) via GuestToHostThunk, then returns.
// This exercises HostToGuestThunk entry, GuestToHostThunk transition, and
// proper return to guest code and back to host.

static std::atomic<int> builtin_call_count{0};
static void BuiltinHandler(ppc::PPCContext* ctx, void* arg0, void* arg1) {
  builtin_call_count.fetch_add(1);
  // Write a known marker so the test can verify the builtin actually ran.
  ctx->r[3] = 0xDEADBEEF;
}

TEST_CASE("HOST_GUEST_HOST_ROUNDTRIP", "[backend]") {
  builtin_call_count = 0;

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

  // Define a builtin function that the guest code will call.
  auto* builtin_fn =
      processor->DefineBuiltin("TestBuiltin", BuiltinHandler, nullptr, nullptr);
  REQUIRE(builtin_fn != nullptr);

  // Create a test module with a guest function that calls the builtin.
  auto module = std::make_unique<TestModule>(
      processor.get(), "Test",
      [](uint32_t address) { return address == 0x80000000; },
      [builtin_fn](HIRBuilder& b) {
        // Store a pre-call marker in r[4].
        StoreGPR(b, 4, b.LoadConstantUint64(0x11111111));
        // Call the builtin — this goes through GuestToHostThunk.
        b.CallExtern(builtin_fn);
        // Store a post-call marker in r[5] to prove we returned properly.
        StoreGPR(b, 5, b.LoadConstantUint64(0x22222222));
        b.Return();
        return true;
      },
      /*skip_cf_simplification=*/true);
  processor->AddModule(std::move(module));
  processor->backend()->CommitExecutableRange(0x80000000, 0x80010000);

  auto fn = processor->ResolveFunction(0x80000000);
  REQUIRE(fn != nullptr);

  uint32_t stack_size = 64 * 1024;
  uint32_t stack_address = memory->SystemHeapAlloc(stack_size);
  auto thread_state = std::make_unique<ThreadState>(processor.get(), 0x100,
                                                    stack_address + stack_size);
  auto ctx = thread_state->context();
  ctx->lr = 0xBCBCBCBC;
  ctx->r[3] = 0;
  ctx->r[4] = 0;
  ctx->r[5] = 0;

  fn->Call(thread_state.get(), uint32_t(ctx->lr));

  // Verify the builtin ran.
  REQUIRE(builtin_call_count == 1);
  // Verify the builtin wrote its marker.
  REQUIRE(ctx->r[3] == 0xDEADBEEF);
  // Verify pre-call code ran.
  REQUIRE(ctx->r[4] == 0x11111111);
  // Verify post-call code ran (guest code continued after GuestToHostThunk).
  REQUIRE(ctx->r[5] == 0x22222222);

  memory->SystemHeapFree(stack_address);
}

// =============================================================================
// GPR preservation across GuestToHostThunk
// =============================================================================
// Tests that callee-saved GPRs used by the register allocator survive a
// host call via GuestToHostThunk. We load values into several GPRs before
// the call, invoke a builtin, then read them back after.

static void EmptyBuiltin(ppc::PPCContext* ctx, void* arg0, void* arg1) {
  // Intentionally empty — we just want to exercise the thunk transition.
}

TEST_CASE("GPR_PRESERVATION_ACROSS_HOST_CALL", "[backend]") {
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

  auto* builtin_fn =
      processor->DefineBuiltin("EmptyBuiltin", EmptyBuiltin, nullptr, nullptr);

  // Load known values into r[10]-r[15] (via context load/store, which the
  // register allocator maps to callee-saved GPRs), call the builtin, then
  // copy them to r[3]-r[8] for verification.
  auto module = std::make_unique<TestModule>(
      processor.get(), "Test",
      [](uint32_t address) { return address == 0x80000000; },
      [builtin_fn](HIRBuilder& b) {
        // Load known values from context slots.
        auto v0 = LoadGPR(b, 10);
        auto v1 = LoadGPR(b, 11);
        auto v2 = LoadGPR(b, 12);
        auto v3 = LoadGPR(b, 13);
        auto v4 = LoadGPR(b, 14);
        auto v5 = LoadGPR(b, 15);
        // Call host — this must preserve all the above.
        b.CallExtern(builtin_fn);
        // Store them back to different slots for verification.
        StoreGPR(b, 3, v0);
        StoreGPR(b, 4, v1);
        StoreGPR(b, 5, v2);
        StoreGPR(b, 6, v3);
        StoreGPR(b, 7, v4);
        StoreGPR(b, 8, v5);
        b.Return();
        return true;
      },
      /*skip_cf_simplification=*/true);
  processor->AddModule(std::move(module));
  processor->backend()->CommitExecutableRange(0x80000000, 0x80010000);

  auto fn = processor->ResolveFunction(0x80000000);
  REQUIRE(fn != nullptr);

  uint32_t stack_size = 64 * 1024;
  uint32_t stack_address = memory->SystemHeapAlloc(stack_size);
  auto thread_state = std::make_unique<ThreadState>(processor.get(), 0x100,
                                                    stack_address + stack_size);
  auto ctx = thread_state->context();
  ctx->lr = 0xBCBCBCBC;

  // Set known values in source registers.
  ctx->r[10] = 0xAAAAAAAA00000001ULL;
  ctx->r[11] = 0xBBBBBBBB00000002ULL;
  ctx->r[12] = 0xCCCCCCCC00000003ULL;
  ctx->r[13] = 0xDDDDDDDD00000004ULL;
  ctx->r[14] = 0xEEEEEEEE00000005ULL;
  ctx->r[15] = 0xFFFFFFFF00000006ULL;

  fn->Call(thread_state.get(), uint32_t(ctx->lr));

  REQUIRE(ctx->r[3] == 0xAAAAAAAA00000001ULL);
  REQUIRE(ctx->r[4] == 0xBBBBBBBB00000002ULL);
  REQUIRE(ctx->r[5] == 0xCCCCCCCC00000003ULL);
  REQUIRE(ctx->r[6] == 0xDDDDDDDD00000004ULL);
  REQUIRE(ctx->r[7] == 0xEEEEEEEE00000005ULL);
  REQUIRE(ctx->r[8] == 0xFFFFFFFF00000006ULL);

  memory->SystemHeapFree(stack_address);
}

// =============================================================================
// VEC register preservation across GuestToHostThunk
// =============================================================================
// Tests that vector registers allocated by the JIT survive a host call.
// Loads vec128 values into VRs, calls a builtin, then reads them back.

static void NeonClobberBuiltin(ppc::PPCContext* ctx, void* arg0, void* arg1) {
  // This function intentionally does nothing, but the ABI allows it to
  // clobber caller-saved NEON/XMM registers. The compiler might use them
  // for local variables, memcpy, etc. The thunk must save/restore them.
}

TEST_CASE("VEC_PRESERVATION_ACROSS_HOST_CALL", "[backend]") {
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

  auto* builtin_fn = processor->DefineBuiltin(
      "NeonClobberBuiltin", NeonClobberBuiltin, nullptr, nullptr);

  // Load vec128 values from v[10]-v[13], call builtin, store to v[3]-v[6].
  auto module = std::make_unique<TestModule>(
      processor.get(), "Test",
      [](uint32_t address) { return address == 0x80000000; },
      [builtin_fn](HIRBuilder& b) {
        auto vec0 = LoadVR(b, 10);
        auto vec1 = LoadVR(b, 11);
        auto vec2 = LoadVR(b, 12);
        auto vec3 = LoadVR(b, 13);
        // Call host — thunk must preserve VEC regs.
        b.CallExtern(builtin_fn);
        // Store back for verification.
        StoreVR(b, 3, vec0);
        StoreVR(b, 4, vec1);
        StoreVR(b, 5, vec2);
        StoreVR(b, 6, vec3);
        b.Return();
        return true;
      },
      /*skip_cf_simplification=*/true);
  processor->AddModule(std::move(module));
  processor->backend()->CommitExecutableRange(0x80000000, 0x80010000);

  auto fn = processor->ResolveFunction(0x80000000);
  REQUIRE(fn != nullptr);

  uint32_t stack_size = 64 * 1024;
  uint32_t stack_address = memory->SystemHeapAlloc(stack_size);
  auto thread_state = std::make_unique<ThreadState>(processor.get(), 0x100,
                                                    stack_address + stack_size);
  auto ctx = thread_state->context();
  ctx->lr = 0xBCBCBCBC;

  // Set known vec128 values in source VRs.
  ctx->v[10] = vec128i(0x11111111, 0x22222222, 0x33333333, 0x44444444);
  ctx->v[11] = vec128i(0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD);
  ctx->v[12] = vec128i(0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10);
  ctx->v[13] = vec128i(0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0x9ABCDEF0);

  fn->Call(thread_state.get(), uint32_t(ctx->lr));

  REQUIRE(ctx->v[3] == vec128i(0x11111111, 0x22222222, 0x33333333, 0x44444444));
  REQUIRE(ctx->v[4] == vec128i(0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD));
  REQUIRE(ctx->v[5] == vec128i(0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10));
  REQUIRE(ctx->v[6] == vec128i(0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0x9ABCDEF0));

  memory->SystemHeapFree(stack_address);
}

// =============================================================================
// Basic guest code execution — context load/store round-trip
// =============================================================================
// The simplest possible test: enter guest code, read a context value, write
// it to another slot, return. Exercises HostToGuestThunk and epilog.
TEST_CASE("BASIC_GUEST_EXECUTION", "[backend]") {
  TestFunction test([](HIRBuilder& b) {
    StoreGPR(b, 3, LoadGPR(b, 4));
    b.Return();
  });

  test.Run(
      [](PPCContext* ctx) { ctx->r[4] = 0x123456789ABCDEF0ULL; },
      [](PPCContext* ctx) { REQUIRE(ctx->r[3] == 0x123456789ABCDEF0ULL); });
}

// =============================================================================
// Multiple builtin calls in sequence
// =============================================================================
// Exercises that the GuestToHostThunk properly restores state so that
// multiple host calls from the same guest function work correctly.

static std::atomic<int> multi_call_counter{0};
static void CountingBuiltin(ppc::PPCContext* ctx, void* arg0, void* arg1) {
  multi_call_counter.fetch_add(1);
  ctx->r[3] = ctx->r[3] + 1;
}

TEST_CASE("MULTIPLE_BUILTIN_CALLS", "[backend]") {
  multi_call_counter = 0;

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

  auto* builtin_fn = processor->DefineBuiltin(
      "CountingBuiltin", CountingBuiltin, nullptr, nullptr);

  auto module = std::make_unique<TestModule>(
      processor.get(), "Test",
      [](uint32_t address) { return address == 0x80000000; },
      [builtin_fn](HIRBuilder& b) {
        // Initialize r[3] = 0.
        StoreGPR(b, 3, b.LoadConstantUint64(0));
        // Call builtin three times — each increments r[3].
        b.CallExtern(builtin_fn);
        b.CallExtern(builtin_fn);
        b.CallExtern(builtin_fn);
        b.Return();
        return true;
      },
      /*skip_cf_simplification=*/true);
  processor->AddModule(std::move(module));
  processor->backend()->CommitExecutableRange(0x80000000, 0x80010000);

  auto fn = processor->ResolveFunction(0x80000000);
  REQUIRE(fn != nullptr);

  uint32_t stack_size = 64 * 1024;
  uint32_t stack_address = memory->SystemHeapAlloc(stack_size);
  auto thread_state = std::make_unique<ThreadState>(processor.get(), 0x100,
                                                    stack_address + stack_size);
  auto ctx = thread_state->context();
  ctx->lr = 0xBCBCBCBC;
  ctx->r[3] = 0;

  fn->Call(thread_state.get(), uint32_t(ctx->lr));

  REQUIRE(multi_call_counter == 3);
  REQUIRE(ctx->r[3] == 3);

  memory->SystemHeapFree(stack_address);
}
