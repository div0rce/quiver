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

// Nibble variant for 64-bit-lane compaction. For each 4-bit selection value, the lane
// indices of set bits (front-packed, remainder ascending) ALREADY EXPANDED to the epi32
// index pairs {2p, 2p+1} that vpermd consumes — AVX2 has no 64-bit variable permute, so
// 64-bit lanes are moved as adjacent 32-bit pairs.
//
// Storing the expanded form (rather than 4 lane indices expanded per call) lets the backend
// load the control vector with a single aligned 32-byte load, exactly like the 32-bit path.
// Building it per nibble with _mm256_setr_epi32 instead costs 8 scalar loads plus the insert
// chain, measured at 3.48x slower for the compaction loop in isolation. Same 512 B either way.
// Each row is 32 B and the table is 64 B aligned, so every row is 32 B aligned (vmovdqa-safe).
struct CompactLut64 {
  alignas(64) std::uint32_t perm[16][8];
};
extern const CompactLut64 kCompactLut64;  // 512 B

// Popcount-per-byte table (advance amounts for the compaction cursor).
struct PopcountLut {
  alignas(64) std::uint8_t count[256];
};
extern const PopcountLut kPopcountLut;  // 256 B

// --- NEON TBL nibble tables (REQ-SIMD-005 "16-entry nibble TBL tables"; simdprune lineage,
// --- Survey §4.1). Each row is a byte-shuffle control: the byte indices of the selected
// --- lanes, front-packed; 0xFF fills the remainder (TBL yields 0 for out-of-range indices —
// --- scratch bytes past the cursor stay inside the REQ-MEM-008 capacity region). The
// --- control width follows the lane width (8 bytes for the 8-byte groups of 8/16-bit lanes,
// --- 16 bytes for the 128-bit groups of 32/64-bit lanes — a documented interpretation of
// --- the REQ's "8-byte" wording, recorded in gate M5).

struct CompactNib8 {  // nibble selects 4 one-byte lanes (control padded to 8)
  alignas(64) std::uint8_t ctrl[16][8];
};
extern const CompactNib8 kCompactNib8;  // 128 B

struct CompactNib16 {  // nibble selects 4 two-byte lanes
  alignas(64) std::uint8_t ctrl[16][8];
};
extern const CompactNib16 kCompactNib16;  // 128 B

struct CompactNib32 {  // nibble selects 4 four-byte lanes (full 128-bit control)
  alignas(64) std::uint8_t ctrl[16][16];
};
extern const CompactNib32 kCompactNib32;  // 256 B

struct CompactPair64 {  // 2-bit value selects 2 eight-byte lanes (full 128-bit control)
  alignas(64) std::uint8_t ctrl[4][16];
};
extern const CompactPair64 kCompactPair64;  // 64 B

}  // namespace detail
QUIVER_END_NAMESPACE
