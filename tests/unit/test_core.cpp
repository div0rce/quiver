// MOD-CORE validation: Surface B inventory shape, concept accept/reject, SumType mapping,
// assert-macro behavior.
// Covers: REQ-CORE-001..003, REQ-API-001/-004/-005, REQ-CORE-002 triviality (PRD 05 §3)
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

#include "quiver/core.h"

namespace {

using quiver::BatchView;
using quiver::BitmapView;
using quiver::CodeType;
using quiver::Element;
using quiver::IntElement;
using quiver::Isa;
using quiver::SelVec;
using quiver::Sma;
using quiver::SumType;

// --- REQ-API-004: exactly the ten element types -------------------------------------------
static_assert(Element<std::int8_t> && Element<std::int16_t> && Element<std::int32_t> &&
              Element<std::int64_t> && Element<std::uint8_t> && Element<std::uint16_t> &&
              Element<std::uint32_t> && Element<std::uint64_t> && Element<float> &&
              Element<double>);
static_assert(!Element<bool> && !Element<char> && !Element<void*> && !Element<const int*> &&
              !Element<long double>);
static_assert(IntElement<std::int32_t> && !IntElement<float> && !IntElement<bool>);
static_assert(CodeType<std::uint8_t> && CodeType<std::uint16_t> && CodeType<std::uint32_t>);
static_assert(!CodeType<std::uint64_t> && !CodeType<std::int8_t> && !CodeType<bool>);

// --- REQ-CORE-002: views are trivially copyable, standard layout, ctor-free ----------------
static_assert(std::is_trivially_copyable_v<BatchView<std::int32_t>> &&
              std::is_standard_layout_v<BatchView<std::int32_t>>);
static_assert(std::is_trivially_copyable_v<BitmapView> && std::is_standard_layout_v<BitmapView>);
static_assert(std::is_trivially_copyable_v<SelVec> && std::is_standard_layout_v<SelVec>);
static_assert(std::is_trivially_copyable_v<Sma<double>> && std::is_standard_layout_v<Sma<double>>);
static_assert(std::is_aggregate_v<BatchView<float>> && std::is_aggregate_v<SelVec>);

// Documented layout: 16 bytes on LP64 (PRD 05 §3 invariant).
static_assert(sizeof(void*) != 8 || sizeof(BatchView<std::int64_t>) == 16);
static_assert(sizeof(void*) != 8 || sizeof(SelVec) == 16);

// --- Frozen enum values: dispatch tables index by Isa (PRD 04 §3) ---------------------------
static_assert(static_cast<int>(Isa::kScalar) == 0 && static_cast<int>(Isa::kNeon) == 1 &&
              static_cast<int>(Isa::kAvx2) == 2 && static_cast<int>(Isa::kAvx512) == 3);
static_assert(static_cast<int>(quiver::CompareOp::kEq) == 0 &&
              static_cast<int>(quiver::CompareOp::kGe) == 5);

// --- SumType mapping (PRD 04 §3) ------------------------------------------------------------
static_assert(std::is_same_v<SumType<std::int8_t>, std::int64_t> &&
              std::is_same_v<SumType<std::int64_t>, std::int64_t>);
static_assert(std::is_same_v<SumType<std::uint16_t>, std::uint64_t> &&
              std::is_same_v<SumType<std::uint64_t>, std::uint64_t>);
static_assert(std::is_same_v<SumType<float>, float> && std::is_same_v<SumType<double>, double>);

// --- kMaxBatchLen (REQ-API-005) --------------------------------------------------------------
static_assert(quiver::kMaxBatchLen == 2'147'483'647);

TEST(Core, ViewsAreDumbAggregates) {
  // Views carry no invariants at construction (REQ-CORE-002); contracts live at kernel
  // boundaries.
  const std::int32_t values[4] = {1, 2, 3, 4};
  const BatchView<std::int32_t> b{values, 4};
  EXPECT_EQ(b.data, values);
  EXPECT_EQ(b.len, 4);
  const BitmapView all_valid{nullptr};  // null = all-valid where the parameter is `validity`
  EXPECT_EQ(all_valid.bits, nullptr);
}

// --- REQ-CORE-003 / REQ-ERR-002: assert behavior ---------------------------------------------
TEST(Core, AssertMacroFailsWithContractFormatWhenEnabled) {
  if (!quiver::detail::assertions_enabled()) {
    GTEST_SKIP() << "library built without QUIVER_ENABLE_ASSERTS (REQ-TEST-014 self-skip)";
  }
#if defined(QUIVER_ENABLE_ASSERTS)
  // Format contract: "<file>:<line>: assertion: <msg>" (REQ-ERR-002).
  EXPECT_DEATH(QUIVER_ASSERT(false, "test_core: forced failure [REQ-CORE-003]"),
               "assertion: test_core: forced failure \\[REQ-CORE-003\\]");
  QUIVER_ASSERT(true, "never fires");
#else
  GTEST_SKIP() << "test binary compiled without QUIVER_ENABLE_ASSERTS";
#endif
}

TEST(Core, AssertMacroIsNoopWhenDisabled) {
#if defined(QUIVER_ENABLE_ASSERTS)
  GTEST_SKIP() << "asserts enabled in this build; no-op behavior covered by release presets";
#else
  QUIVER_ASSERT(false, "must not fire when disabled");
  SUCCEED();
#endif
}

}  // namespace
