// K8 differential matrix: every host backend vs the independent ADR-026 oracle over the
// EXHAUSTIVE width sweep 0..8*sizeof(Out) (REQ-K8-004), byte-exact.
// Covers: REQ-TEST-002/-003, REQ-K8-001..004
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"
#include "quiver/unpack.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

template <class Body>
void for_each_backend(Body body) {
  for (const quiver::Isa isa :
       {quiver::Isa::kScalar, quiver::Isa::kNeon, quiver::Isa::kAvx2, quiver::Isa::kAvx512}) {
    if (isa != quiver::Isa::kScalar && !quiver::cpu_supports(isa)) {
      continue;
    }
    ASSERT_TRUE(quiver::set_isa_override(isa));
    body();
  }
  quiver::clear_isa_override();
}

template <class Out>
void run_unpack_diff(std::uint64_t seed) {
  Rng rng(seed);
  const int max_w = 8 * static_cast<int>(sizeof(Out));
  for (int w = 0; w <= max_w; ++w) {  // exhaustive (REQ-K8-004)
    for (const std::int64_t n : {0LL, 1LL, 7LL, 33LL, 257LL}) {
      std::vector<std::uint8_t> packed((static_cast<std::size_t>(n) * w + 7) / 8 + 1);
      for (auto& byte : packed) {
        byte = static_cast<std::uint8_t>(rng.next());
      }
      std::vector<Out> got(static_cast<std::size_t>(n) + 1);
      const auto base = static_cast<Out>(rng.next());
      quiver::unpack_for(w == 0 ? nullptr : packed.data(), n, w, base, got.data());
      for (std::int64_t i = 0; i < n; ++i) {
        const Out want = static_cast<Out>(
            base + (w == 0 ? Out{0} : ref::unpack_value_expected<Out>(packed.data(), i, w)));
        ASSERT_EQ(got[static_cast<std::size_t>(i)], want)
            << "Out=" << sizeof(Out) << " w=" << w << " n=" << n << " i=" << i;
      }
    }
  }
}

TEST(DiffUnpack, AllBackendsMatchOracleExhaustiveWidths) {
  for_each_backend([&] {
    run_unpack_diff<std::uint8_t>(0xD1FF0801ull);
    run_unpack_diff<std::uint16_t>(0xD1FF0802ull);
    run_unpack_diff<std::uint32_t>(0xD1FF0803ull);
    run_unpack_diff<std::uint64_t>(0xD1FF0804ull);
  });
}

}  // namespace
