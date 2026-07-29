// K8 sub-byte NEON unpack candidate: correctness + bounds, tested DIRECTLY (not via dispatch).
// The candidate is experimental and not on production dispatch (see unpack_neon_candidate.h), so
// this test calls it by name to give CI coverage on aarch64 runners. Two guarantees:
//   1. Bit-exactness vs the independent ADR-026 oracle, over sub-byte widths 1..7, every Out width,
//      and boundary lengths around the 8-value block edge.
//   2. No read past ceil(n*w/8) and no write past n (REQ-K8-002 / REQ-SEC-004), via guard pages.
// Covers: REQ-K8-001..004, REQ-SEC-004, REQ-TEST-002/-003/-006
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/core.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

#if defined(__aarch64__) || defined(_M_ARM64)
#include "src/kernels/unpack/unpack_neon_candidate.h"

namespace {

using quiver_test::Guard;
using quiver_test::GuardedBuffer;
using quiver_test::Rng;
namespace ref = quiver_test::ref;

// Boundary lengths around the 8-value (byte-aligned) block edge, per the correctness spec.
constexpr std::int64_t kBoundaryN[] = {0, 1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 257};

// Widths run 1..7 and must also fit the output type — a bound that only binds for 1-byte outputs.
template <class Out>
constexpr int max_width() {
  constexpr int kBits = 8 * static_cast<int>(sizeof(Out));
  return kBits < 7 ? kBits : 7;
}

void fill_random(std::uint8_t* bytes, std::size_t count, Rng& rng) {
  for (std::size_t i = 0; i < count; ++i) {
    bytes[i] = static_cast<std::uint8_t>(rng.next());
  }
}

// One unpack case: the packed input, how many values it holds, at what width, over what base.
template <class Out>
struct SubbyteCase {
  const std::uint8_t* packed;
  std::int64_t n;
  int w;
  Out base;
};

// Every case ends the same way: each unpacked value equals base + the oracle's value.
template <class Out>
void expect_matches_oracle(const SubbyteCase<Out>& c, const Out* got, const char* label) {
  for (std::int64_t i = 0; i < c.n; ++i) {
    const Out want = static_cast<Out>(c.base + ref::unpack_value_expected<Out>(c.packed, i, c.w));
    ASSERT_EQ(got[i], want) << label << " Out=" << sizeof(Out) << " w=" << c.w << " n=" << c.n
                            << " i=" << i;
  }
}

// Slack-padded buffers: a spare packed byte and a spare output element, both left poisoned.
template <class Out>
void run_padded_case(std::int64_t n, int w, Rng& rng, const char* label) {
  const std::size_t nbytes = static_cast<std::size_t>((n * w + 7) / 8) + 1;  // +1 slack pad
  std::vector<std::uint8_t> packed(nbytes);
  fill_random(packed.data(), packed.size(), rng);
  const auto base = static_cast<Out>(rng.next());
  std::vector<Out> got(static_cast<std::size_t>(n) + 1, Out{0xAB});
  quiver::detail::neon::unpack_subbyte_candidate<Out>(packed.data(), n, w, base, got.data());
  expect_matches_oracle<Out>({packed.data(), n, w, base}, got.data(), label);
}

// Input packed buffer sized to EXACTLY ceil(n*w/8) with a guard page at its end, and output sized
// to EXACTLY n with a guard page at its end: any over-read or over-write faults.
template <class Out>
void run_bounded_case(std::int64_t n, int w, Rng& rng) {
  const std::int64_t nbytes = (n * w + 7) / 8;  // exact contract bound
  GuardedBuffer<std::uint8_t> packed(nbytes, Guard::kEnd);
  fill_random(packed.data(), static_cast<std::size_t>(nbytes), rng);
  GuardedBuffer<Out> got(n, Guard::kEnd);
  const auto base = static_cast<Out>(rng.next());
  quiver::detail::neon::unpack_subbyte_candidate<Out>(packed.data(), n, w, base, got.data());
  // Correctness under the tight allocation too (reads must have been in-bounds to be correct).
  expect_matches_oracle<Out>({packed.data(), n, w, base}, got.data(), "bounded");
}

template <class Out>
void check_bit_exact(std::uint64_t seed) {
  Rng rng(seed);
  for (int w = 1; w <= max_width<Out>(); ++w) {
    for (const std::int64_t n : kBoundaryN) {
      run_padded_case<Out>(n, w, rng, "boundary");
    }
  }
}

template <class Out>
void check_no_over_read(std::uint64_t seed) {
  Rng rng(seed);
  for (int w = 1; w <= max_width<Out>(); ++w) {
    for (const std::int64_t n : kBoundaryN) {
      if (n != 0) {  // n == 0 has no bytes to bound
        run_bounded_case<Out>(n, w, rng);
      }
    }
  }
}

// Seeded randomized differential sweep: the ground a fuzzer would cover (random width, length, and
// packed bytes), running in CI on the arm64 leg. libFuzzer itself cannot reach this aarch64-only
// candidate (the CI fuzz leg is x86), so this stands in for coverage-guided fuzzing of the tails
// and arbitrary lengths.
template <class Out>
void check_random(std::uint64_t seed) {
  Rng rng(seed);
  for (int iter = 0; iter < 4000; ++iter) {
    const int w = 1 + static_cast<int>(rng.next() % 7);  // width in [1,7]
    if (w > max_width<Out>()) {
      continue;
    }
    const std::int64_t n =
        static_cast<std::int64_t>(rng.next() % 300);  // arbitrary length incl tails
    run_padded_case<Out>(n, w, rng, "random");
  }
}

TEST(DiffUnpackSubbyte, CandidateMatchesOracleAllWidthsAndBoundaryLengths) {
  check_bit_exact<std::uint8_t>(0x5B0801ull);
  check_bit_exact<std::uint16_t>(0x5B0802ull);
  check_bit_exact<std::uint32_t>(0x5B0803ull);
  check_bit_exact<std::uint64_t>(0x5B0804ull);
}

TEST(DiffUnpackSubbyte, CandidateMatchesOracleRandomizedSweep) {
  check_random<std::uint8_t>(0x7A0801ull);
  check_random<std::uint16_t>(0x7A0802ull);
  check_random<std::uint32_t>(0x7A0803ull);
  check_random<std::uint64_t>(0x7A0804ull);
}

TEST(DiffUnpackSubbyte, CandidateStaysInBounds) {
  ASSERT_NE(GuardedBuffer<std::uint8_t>(8, Guard::kEnd).data(), nullptr)
      << "guard-page allocation unavailable on this platform";
  check_no_over_read<std::uint8_t>(0x6C0801ull);
  check_no_over_read<std::uint16_t>(0x6C0802ull);
  check_no_over_read<std::uint32_t>(0x6C0803ull);
  check_no_over_read<std::uint64_t>(0x6C0804ull);
}

}  // namespace

#else  // not aarch64: the candidate does not exist; keep the suite non-empty and green.

TEST(DiffUnpackSubbyte, SkippedOffAarch64) {
  GTEST_SKIP() << "sub-byte NEON unpack candidate is aarch64-only";
}

#endif
