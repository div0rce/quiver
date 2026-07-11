// A small kernel pipeline in the convenience spelling (ADR-027): compare into a selection
// vector (K1), gather the selected values (K5), and reduce them (K6). Storage stays explicit
// and caller-owned; results return as the written subspan, so counts travel with the data and
// no view-construction or size bookkeeping appears. Ordinary consumer settings (exceptions +
// RTTI, REQ-INT-006). Module: MOD-EXAMPLES
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

#include "quiver/quiver.h"

// --8<-- [start:pipeline]
int main() {
  const std::vector<std::int32_t> in = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};

  // K1: indices of elements >= 5. The output span's capacity is checked (assertion builds);
  // the returned subspan is exactly the matches.
  std::vector<std::uint32_t> selection_storage(in.size());
  const auto selected =
      quiver::compare_selvec(quiver::CompareOp::kGe, in, 5, std::span{selection_storage});

  // K5: gather those elements, packed.
  std::vector<std::int32_t> picked_storage(selected.size());
  const auto picked = quiver::take(in, selected, std::span{picked_storage});

  // K6: sum them (wrapping into the wider accumulator).
  const auto sum = quiver::reduce_sum_wrap(picked);

  std::printf("selected %zu elements >= 5; sum = %lld\n", picked.size(),
              static_cast<long long>(sum));
  return 0;
}
// --8<-- [end:pipeline]
