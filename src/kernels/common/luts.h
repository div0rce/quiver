// MOD-KCOMMON: shared compaction lookup tables (REQ-SIMD-005) — consteval-generated,
// runtime-re-derived by tests (PRD 05 §6 invariant). Consumers: the emulated-compress core
// of K1-selvec/K2/K3 on pre-AVX-512 ISAs (PRD 09 §6), first used by the AVX2 backends at M4;
// committed at M3 per the milestone file list. Total budget ≤ 16 KiB.
// Module: MOD-KCOMMON | REQs: REQ-SIMD-005 | ADR-003, PRD 09 §6
#pragma once

#include <cstdint>

#include "quiver/detail/config.h"

QUIVER_BEGIN_NAMESPACE
namespace detail {

// For each selection byte b (256 values): the lane indices of b's set bits, in ascending
// order, front-packed; remaining lanes hold 0..7 ascending from the unset bits (don't-care
// for compaction — stores past the popcount land in the REQ-MEM-008 capacity region).
// Used as vpermd/TBL permutation indices for 32-bit (and index-stream) compaction.
struct CompactLut32 {
  alignas(64) std::uint32_t perm[256][8];
};
extern const CompactLut32 kCompactLut32;  // constinit-built in luts.cpp (8 KiB)

// Nibble variant for 64-bit-lane compaction (vpermq) and NEON TBL half-vectors: for each
// 4-bit selection value, the lane indices of set bits, front-packed; remainder ascending.
struct CompactLut64 {
  alignas(64) std::uint64_t perm[16][4];
};
extern const CompactLut64 kCompactLut64;  // 512 B

// Popcount-per-byte table (advance amounts for the compaction cursor).
struct PopcountLut {
  alignas(64) std::uint8_t count[256];
};
extern const PopcountLut kPopcountLut;  // 256 B

}  // namespace detail
QUIVER_END_NAMESPACE
