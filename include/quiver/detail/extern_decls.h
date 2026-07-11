// The single X-macro inventory of concrete kernel symbols: X(uid, ret, name, params, args).
// It drives BOTH the dispatched-wrapper declarations below AND the dispatch entry table in
// dispatch_tables.cpp — table/declaration consistency by construction (PRD 07 §4,
// REQ-KERNEL-005). Concrete symbols are overload sets on element/code types; the public
// template facades resolve to them by ordinary overload resolution (ADR-006).
// GENERATED-BY-HAND-STABLE (PRD 02 §3): regenerate only with a documented inventory change.
// Tier A families (K1–K6) populated at M3; Tier B (K7–K10) populated at M6.
// Module: MOD-CORE (declarations) / MOD-DISPATCH (table emission) | ADR-006
#pragma once

#include "quiver/core.h"

#define QUIVER_KERNEL_ENTRY_LIST(X)                                                                \
  X(k1_cmp1_bm_i8, std::int64_t, k1_compare_bitmap,                                                \
    (CompareOp op, const std::int8_t* in, std::int64_t n, std::int8_t comparand,                   \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_i8, std::int64_t, k1_compare_bitmap2,                                               \
    (CompareOp op, const std::int8_t* a, const std::int8_t* b, std::int64_t n,                     \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_i8, std::int64_t, k1_compare_between_bitmap,                                         \
    (const std::int8_t* in, std::int64_t n, std::int8_t lo, std::int8_t hi,                        \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_i8, std::int64_t, k1_compare_selvec,                                                \
    (CompareOp op, const std::int8_t* in, std::int64_t n, std::int8_t comparand,                   \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_i8, std::int64_t, k1_compare_selvec2,                                               \
    (CompareOp op, const std::int8_t* a, const std::int8_t* b, std::int64_t n,                     \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_i8, std::int64_t, k1_compare_between_selvec,                                         \
    (const std::int8_t* in, std::int64_t n, std::int8_t lo, std::int8_t hi,                        \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_i16, std::int64_t, k1_compare_bitmap,                                               \
    (CompareOp op, const std::int16_t* in, std::int64_t n, std::int16_t comparand,                 \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_i16, std::int64_t, k1_compare_bitmap2,                                              \
    (CompareOp op, const std::int16_t* a, const std::int16_t* b, std::int64_t n,                   \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_i16, std::int64_t, k1_compare_between_bitmap,                                        \
    (const std::int16_t* in, std::int64_t n, std::int16_t lo, std::int16_t hi,                     \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_i16, std::int64_t, k1_compare_selvec,                                               \
    (CompareOp op, const std::int16_t* in, std::int64_t n, std::int16_t comparand,                 \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_i16, std::int64_t, k1_compare_selvec2,                                              \
    (CompareOp op, const std::int16_t* a, const std::int16_t* b, std::int64_t n,                   \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_i16, std::int64_t, k1_compare_between_selvec,                                        \
    (const std::int16_t* in, std::int64_t n, std::int16_t lo, std::int16_t hi,                     \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_i32, std::int64_t, k1_compare_bitmap,                                               \
    (CompareOp op, const std::int32_t* in, std::int64_t n, std::int32_t comparand,                 \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_i32, std::int64_t, k1_compare_bitmap2,                                              \
    (CompareOp op, const std::int32_t* a, const std::int32_t* b, std::int64_t n,                   \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_i32, std::int64_t, k1_compare_between_bitmap,                                        \
    (const std::int32_t* in, std::int64_t n, std::int32_t lo, std::int32_t hi,                     \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_i32, std::int64_t, k1_compare_selvec,                                               \
    (CompareOp op, const std::int32_t* in, std::int64_t n, std::int32_t comparand,                 \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_i32, std::int64_t, k1_compare_selvec2,                                              \
    (CompareOp op, const std::int32_t* a, const std::int32_t* b, std::int64_t n,                   \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_i32, std::int64_t, k1_compare_between_selvec,                                        \
    (const std::int32_t* in, std::int64_t n, std::int32_t lo, std::int32_t hi,                     \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_i64, std::int64_t, k1_compare_bitmap,                                               \
    (CompareOp op, const std::int64_t* in, std::int64_t n, std::int64_t comparand,                 \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_i64, std::int64_t, k1_compare_bitmap2,                                              \
    (CompareOp op, const std::int64_t* a, const std::int64_t* b, std::int64_t n,                   \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_i64, std::int64_t, k1_compare_between_bitmap,                                        \
    (const std::int64_t* in, std::int64_t n, std::int64_t lo, std::int64_t hi,                     \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_i64, std::int64_t, k1_compare_selvec,                                               \
    (CompareOp op, const std::int64_t* in, std::int64_t n, std::int64_t comparand,                 \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_i64, std::int64_t, k1_compare_selvec2,                                              \
    (CompareOp op, const std::int64_t* a, const std::int64_t* b, std::int64_t n,                   \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_i64, std::int64_t, k1_compare_between_selvec,                                        \
    (const std::int64_t* in, std::int64_t n, std::int64_t lo, std::int64_t hi,                     \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_u8, std::int64_t, k1_compare_bitmap,                                                \
    (CompareOp op, const std::uint8_t* in, std::int64_t n, std::uint8_t comparand,                 \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_u8, std::int64_t, k1_compare_bitmap2,                                               \
    (CompareOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n,                   \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_u8, std::int64_t, k1_compare_between_bitmap,                                         \
    (const std::uint8_t* in, std::int64_t n, std::uint8_t lo, std::uint8_t hi,                     \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_u8, std::int64_t, k1_compare_selvec,                                                \
    (CompareOp op, const std::uint8_t* in, std::int64_t n, std::uint8_t comparand,                 \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_u8, std::int64_t, k1_compare_selvec2,                                               \
    (CompareOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n,                   \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_u8, std::int64_t, k1_compare_between_selvec,                                         \
    (const std::uint8_t* in, std::int64_t n, std::uint8_t lo, std::uint8_t hi,                     \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_u16, std::int64_t, k1_compare_bitmap,                                               \
    (CompareOp op, const std::uint16_t* in, std::int64_t n, std::uint16_t comparand,               \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_u16, std::int64_t, k1_compare_bitmap2,                                              \
    (CompareOp op, const std::uint16_t* a, const std::uint16_t* b, std::int64_t n,                 \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_u16, std::int64_t, k1_compare_between_bitmap,                                        \
    (const std::uint16_t* in, std::int64_t n, std::uint16_t lo, std::uint16_t hi,                  \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_u16, std::int64_t, k1_compare_selvec,                                               \
    (CompareOp op, const std::uint16_t* in, std::int64_t n, std::uint16_t comparand,               \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_u16, std::int64_t, k1_compare_selvec2,                                              \
    (CompareOp op, const std::uint16_t* a, const std::uint16_t* b, std::int64_t n,                 \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_u16, std::int64_t, k1_compare_between_selvec,                                        \
    (const std::uint16_t* in, std::int64_t n, std::uint16_t lo, std::uint16_t hi,                  \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_u32, std::int64_t, k1_compare_bitmap,                                               \
    (CompareOp op, const std::uint32_t* in, std::int64_t n, std::uint32_t comparand,               \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_u32, std::int64_t, k1_compare_bitmap2,                                              \
    (CompareOp op, const std::uint32_t* a, const std::uint32_t* b, std::int64_t n,                 \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_u32, std::int64_t, k1_compare_between_bitmap,                                        \
    (const std::uint32_t* in, std::int64_t n, std::uint32_t lo, std::uint32_t hi,                  \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_u32, std::int64_t, k1_compare_selvec,                                               \
    (CompareOp op, const std::uint32_t* in, std::int64_t n, std::uint32_t comparand,               \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_u32, std::int64_t, k1_compare_selvec2,                                              \
    (CompareOp op, const std::uint32_t* a, const std::uint32_t* b, std::int64_t n,                 \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_u32, std::int64_t, k1_compare_between_selvec,                                        \
    (const std::uint32_t* in, std::int64_t n, std::uint32_t lo, std::uint32_t hi,                  \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_u64, std::int64_t, k1_compare_bitmap,                                               \
    (CompareOp op, const std::uint64_t* in, std::int64_t n, std::uint64_t comparand,               \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_u64, std::int64_t, k1_compare_bitmap2,                                              \
    (CompareOp op, const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,                 \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_u64, std::int64_t, k1_compare_between_bitmap,                                        \
    (const std::uint64_t* in, std::int64_t n, std::uint64_t lo, std::uint64_t hi,                  \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_u64, std::int64_t, k1_compare_selvec,                                               \
    (CompareOp op, const std::uint64_t* in, std::int64_t n, std::uint64_t comparand,               \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_u64, std::int64_t, k1_compare_selvec2,                                              \
    (CompareOp op, const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,                 \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_u64, std::int64_t, k1_compare_between_selvec,                                        \
    (const std::uint64_t* in, std::int64_t n, std::uint64_t lo, std::uint64_t hi,                  \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_f32, std::int64_t, k1_compare_bitmap,                                               \
    (CompareOp op, const float* in, std::int64_t n, float comparand, const std::uint8_t* validity, \
     std::uint8_t* out),                                                                           \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_f32, std::int64_t, k1_compare_bitmap2,                                              \
    (CompareOp op, const float* a, const float* b, std::int64_t n, const std::uint8_t* a_validity, \
     const std::uint8_t* b_validity, std::uint8_t* out),                                           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_f32, std::int64_t, k1_compare_between_bitmap,                                        \
    (const float* in, std::int64_t n, float lo, float hi, const std::uint8_t* validity,            \
     std::uint8_t* out),                                                                           \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_f32, std::int64_t, k1_compare_selvec,                                               \
    (CompareOp op, const float* in, std::int64_t n, float comparand, const std::uint8_t* validity, \
     std::uint32_t* out),                                                                          \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_f32, std::int64_t, k1_compare_selvec2,                                              \
    (CompareOp op, const float* a, const float* b, std::int64_t n, const std::uint8_t* a_validity, \
     const std::uint8_t* b_validity, std::uint32_t* out),                                          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_f32, std::int64_t, k1_compare_between_selvec,                                        \
    (const float* in, std::int64_t n, float lo, float hi, const std::uint8_t* validity,            \
     std::uint32_t* out),                                                                          \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_bm_f64, std::int64_t, k1_compare_bitmap,                                               \
    (CompareOp op, const double* in, std::int64_t n, double comparand,                             \
     const std::uint8_t* validity, std::uint8_t* out),                                             \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_bm_f64, std::int64_t, k1_compare_bitmap2,                                              \
    (CompareOp op, const double* a, const double* b, std::int64_t n,                               \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint8_t* out),           \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_bm_f64, std::int64_t, k1_compare_between_bitmap,                                        \
    (const double* in, std::int64_t n, double lo, double hi, const std::uint8_t* validity,         \
     std::uint8_t* out),                                                                           \
    (in, n, lo, hi, validity, out))                                                                \
  X(k1_cmp1_sv_f64, std::int64_t, k1_compare_selvec,                                               \
    (CompareOp op, const double* in, std::int64_t n, double comparand,                             \
     const std::uint8_t* validity, std::uint32_t* out),                                            \
    (op, in, n, comparand, validity, out))                                                         \
  X(k1_cmp2_sv_f64, std::int64_t, k1_compare_selvec2,                                              \
    (CompareOp op, const double* a, const double* b, std::int64_t n,                               \
     const std::uint8_t* a_validity, const std::uint8_t* b_validity, std::uint32_t* out),          \
    (op, a, b, n, a_validity, b_validity, out))                                                    \
  X(k1_btw_sv_f64, std::int64_t, k1_compare_between_selvec,                                        \
    (const double* in, std::int64_t n, double lo, double hi, const std::uint8_t* validity,         \
     std::uint32_t* out),                                                                          \
    (in, n, lo, hi, validity, out))                                                                \
  X(k2_bm_i8, std::int64_t, k2_filter_bitmap,                                                      \
    (const std::int8_t* in, std::int64_t n, const std::uint8_t* selection, std::int8_t* out),      \
    (in, n, selection, out))                                                                       \
  X(k2_sv_i8, std::int64_t, k2_filter_selvec,                                                      \
    (const std::int8_t* in, const std::uint32_t* sel, std::int64_t sel_len, std::int8_t* out),     \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_i16, std::int64_t, k2_filter_bitmap,                                                     \
    (const std::int16_t* in, std::int64_t n, const std::uint8_t* selection, std::int16_t* out),    \
    (in, n, selection, out))                                                                       \
  X(k2_sv_i16, std::int64_t, k2_filter_selvec,                                                     \
    (const std::int16_t* in, const std::uint32_t* sel, std::int64_t sel_len, std::int16_t* out),   \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_i32, std::int64_t, k2_filter_bitmap,                                                     \
    (const std::int32_t* in, std::int64_t n, const std::uint8_t* selection, std::int32_t* out),    \
    (in, n, selection, out))                                                                       \
  X(k2_sv_i32, std::int64_t, k2_filter_selvec,                                                     \
    (const std::int32_t* in, const std::uint32_t* sel, std::int64_t sel_len, std::int32_t* out),   \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_i64, std::int64_t, k2_filter_bitmap,                                                     \
    (const std::int64_t* in, std::int64_t n, const std::uint8_t* selection, std::int64_t* out),    \
    (in, n, selection, out))                                                                       \
  X(k2_sv_i64, std::int64_t, k2_filter_selvec,                                                     \
    (const std::int64_t* in, const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out),   \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_u8, std::int64_t, k2_filter_bitmap,                                                      \
    (const std::uint8_t* in, std::int64_t n, const std::uint8_t* selection, std::uint8_t* out),    \
    (in, n, selection, out))                                                                       \
  X(k2_sv_u8, std::int64_t, k2_filter_selvec,                                                      \
    (const std::uint8_t* in, const std::uint32_t* sel, std::int64_t sel_len, std::uint8_t* out),   \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_u16, std::int64_t, k2_filter_bitmap,                                                     \
    (const std::uint16_t* in, std::int64_t n, const std::uint8_t* selection, std::uint16_t* out),  \
    (in, n, selection, out))                                                                       \
  X(k2_sv_u16, std::int64_t, k2_filter_selvec,                                                     \
    (const std::uint16_t* in, const std::uint32_t* sel, std::int64_t sel_len, std::uint16_t* out), \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_u32, std::int64_t, k2_filter_bitmap,                                                     \
    (const std::uint32_t* in, std::int64_t n, const std::uint8_t* selection, std::uint32_t* out),  \
    (in, n, selection, out))                                                                       \
  X(k2_sv_u32, std::int64_t, k2_filter_selvec,                                                     \
    (const std::uint32_t* in, const std::uint32_t* sel, std::int64_t sel_len, std::uint32_t* out), \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_u64, std::int64_t, k2_filter_bitmap,                                                     \
    (const std::uint64_t* in, std::int64_t n, const std::uint8_t* selection, std::uint64_t* out),  \
    (in, n, selection, out))                                                                       \
  X(k2_sv_u64, std::int64_t, k2_filter_selvec,                                                     \
    (const std::uint64_t* in, const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out), \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_f32, std::int64_t, k2_filter_bitmap,                                                     \
    (const float* in, std::int64_t n, const std::uint8_t* selection, float* out),                  \
    (in, n, selection, out))                                                                       \
  X(k2_sv_f32, std::int64_t, k2_filter_selvec,                                                     \
    (const float* in, const std::uint32_t* sel, std::int64_t sel_len, float* out),                 \
    (in, sel, sel_len, out))                                                                       \
  X(k2_bm_f64, std::int64_t, k2_filter_bitmap,                                                     \
    (const double* in, std::int64_t n, const std::uint8_t* selection, double* out),                \
    (in, n, selection, out))                                                                       \
  X(k2_sv_f64, std::int64_t, k2_filter_selvec,                                                     \
    (const double* in, const std::uint32_t* sel, std::int64_t sel_len, double* out),               \
    (in, sel, sel_len, out))                                                                       \
  X(k3_b2s, std::int64_t, k3_bitmap_to_selvec,                                                     \
    (const std::uint8_t* selection, std::int64_t n, std::uint32_t* out), (selection, n, out))      \
  X(k3_s2b, void, k3_selvec_to_bitmap,                                                             \
    (const std::uint32_t* sel, std::int64_t sel_len, std::int64_t n, std::uint8_t* out),           \
    (sel, sel_len, n, out))                                                                        \
  X(k4_comb, void, k4_mask_combine,                                                                \
    (MaskOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n, std::uint8_t* out),  \
    (op, a, b, n, out))                                                                            \
  X(k4_not, void, k4_mask_not, (const std::uint8_t* a, std::int64_t n, std::uint8_t* out),         \
    (a, n, out))                                                                                   \
  X(k4_pop, std::int64_t, k4_mask_popcount, (const std::uint8_t* a, std::int64_t n), (a, n))       \
  X(k4_all, bool, k4_mask_all, (const std::uint8_t* a, std::int64_t n), (a, n))                    \
  X(k4_any, bool, k4_mask_any, (const std::uint8_t* a, std::int64_t n), (a, n))                    \
  X(k4_none, bool, k4_mask_none, (const std::uint8_t* a, std::int64_t n), (a, n))                  \
  X(k5_take_i8, void, k5_take,                                                                     \
    (const std::int8_t* values, std::int64_t values_len, const std::uint32_t* idx,                 \
     std::int64_t idx_len, std::int8_t* out),                                                      \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_i8_cu8, void, k5_dict_decode,                                                           \
    (const std::int8_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,    \
     const std::uint32_t* sel, std::int64_t sel_len, std::int8_t* out),                            \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_i8_cu16, void, k5_dict_decode,                                                          \
    (const std::int8_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n,   \
     const std::uint32_t* sel, std::int64_t sel_len, std::int8_t* out),                            \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_i8_cu32, void, k5_dict_decode,                                                          \
    (const std::int8_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n,   \
     const std::uint32_t* sel, std::int64_t sel_len, std::int8_t* out),                            \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_i16, void, k5_take,                                                                    \
    (const std::int16_t* values, std::int64_t values_len, const std::uint32_t* idx,                \
     std::int64_t idx_len, std::int16_t* out),                                                     \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_i16_cu8, void, k5_dict_decode,                                                          \
    (const std::int16_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,   \
     const std::uint32_t* sel, std::int64_t sel_len, std::int16_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_i16_cu16, void, k5_dict_decode,                                                         \
    (const std::int16_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::int16_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_i16_cu32, void, k5_dict_decode,                                                         \
    (const std::int16_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::int16_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_i32, void, k5_take,                                                                    \
    (const std::int32_t* values, std::int64_t values_len, const std::uint32_t* idx,                \
     std::int64_t idx_len, std::int32_t* out),                                                     \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_i32_cu8, void, k5_dict_decode,                                                          \
    (const std::int32_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,   \
     const std::uint32_t* sel, std::int64_t sel_len, std::int32_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_i32_cu16, void, k5_dict_decode,                                                         \
    (const std::int32_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::int32_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_i32_cu32, void, k5_dict_decode,                                                         \
    (const std::int32_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::int32_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_i64, void, k5_take,                                                                    \
    (const std::int64_t* values, std::int64_t values_len, const std::uint32_t* idx,                \
     std::int64_t idx_len, std::int64_t* out),                                                     \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_i64_cu8, void, k5_dict_decode,                                                          \
    (const std::int64_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,   \
     const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_i64_cu16, void, k5_dict_decode,                                                         \
    (const std::int64_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_i64_cu32, void, k5_dict_decode,                                                         \
    (const std::int64_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_u8, void, k5_take,                                                                     \
    (const std::uint8_t* values, std::int64_t values_len, const std::uint32_t* idx,                \
     std::int64_t idx_len, std::uint8_t* out),                                                     \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_u8_cu8, void, k5_dict_decode,                                                           \
    (const std::uint8_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,   \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint8_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_u8_cu16, void, k5_dict_decode,                                                          \
    (const std::uint8_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint8_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_u8_cu32, void, k5_dict_decode,                                                          \
    (const std::uint8_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint8_t* out),                           \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_u16, void, k5_take,                                                                    \
    (const std::uint16_t* values, std::int64_t values_len, const std::uint32_t* idx,               \
     std::int64_t idx_len, std::uint16_t* out),                                                    \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_u16_cu8, void, k5_dict_decode,                                                          \
    (const std::uint16_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint16_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_u16_cu16, void, k5_dict_decode,                                                         \
    (const std::uint16_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint16_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_u16_cu32, void, k5_dict_decode,                                                         \
    (const std::uint16_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint16_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_u32, void, k5_take,                                                                    \
    (const std::uint32_t* values, std::int64_t values_len, const std::uint32_t* idx,               \
     std::int64_t idx_len, std::uint32_t* out),                                                    \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_u32_cu8, void, k5_dict_decode,                                                          \
    (const std::uint32_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint32_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_u32_cu16, void, k5_dict_decode,                                                         \
    (const std::uint32_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint32_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_u32_cu32, void, k5_dict_decode,                                                         \
    (const std::uint32_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint32_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_u64, void, k5_take,                                                                    \
    (const std::uint64_t* values, std::int64_t values_len, const std::uint32_t* idx,               \
     std::int64_t idx_len, std::uint64_t* out),                                                    \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_u64_cu8, void, k5_dict_decode,                                                          \
    (const std::uint64_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,  \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_u64_cu16, void, k5_dict_decode,                                                         \
    (const std::uint64_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_u64_cu32, void, k5_dict_decode,                                                         \
    (const std::uint64_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out),                          \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_f32, void, k5_take,                                                                    \
    (const float* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, \
     float* out),                                                                                  \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_f32_cu8, void, k5_dict_decode,                                                          \
    (const float* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,          \
     const std::uint32_t* sel, std::int64_t sel_len, float* out),                                  \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_f32_cu16, void, k5_dict_decode,                                                         \
    (const float* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n,         \
     const std::uint32_t* sel, std::int64_t sel_len, float* out),                                  \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_f32_cu32, void, k5_dict_decode,                                                         \
    (const float* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n,         \
     const std::uint32_t* sel, std::int64_t sel_len, float* out),                                  \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_take_f64, void, k5_take,                                                                    \
    (const double* values, std::int64_t values_len, const std::uint32_t* idx,                      \
     std::int64_t idx_len, double* out),                                                           \
    (values, values_len, idx, idx_len, out))                                                       \
  X(k5_dec_f64_cu8, void, k5_dict_decode,                                                          \
    (const double* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n,         \
     const std::uint32_t* sel, std::int64_t sel_len, double* out),                                 \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_f64_cu16, void, k5_dict_decode,                                                         \
    (const double* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n,        \
     const std::uint32_t* sel, std::int64_t sel_len, double* out),                                 \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k5_dec_f64_cu32, void, k5_dict_decode,                                                         \
    (const double* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n,        \
     const std::uint32_t* sel, std::int64_t sel_len, double* out),                                 \
    (dict, dict_len, codes, n, sel, sel_len, out))                                                 \
  X(k6_min_i8, std::int8_t, k6_reduce_min,                                                         \
    (const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,                          \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_i8, std::int8_t, k6_reduce_max,                                                         \
    (const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,                          \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_i8, std::int64_t, k6_reduce_sum_wrap,                                                  \
    (const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,                          \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_i8, MinMaxSummary<std::int8_t>, k6_compute_sma,                                         \
    (const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,                          \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_i16, std::int16_t, k6_reduce_min,                                                       \
    (const std::int16_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_i16, std::int16_t, k6_reduce_max,                                                       \
    (const std::int16_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_i16, std::int64_t, k6_reduce_sum_wrap,                                                 \
    (const std::int16_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_i16, MinMaxSummary<std::int16_t>, k6_compute_sma,                                       \
    (const std::int16_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_i32, std::int32_t, k6_reduce_min,                                                       \
    (const std::int32_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_i32, std::int32_t, k6_reduce_max,                                                       \
    (const std::int32_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_i32, std::int64_t, k6_reduce_sum_wrap,                                                 \
    (const std::int32_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_i32, MinMaxSummary<std::int32_t>, k6_compute_sma,                                       \
    (const std::int32_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_i64, std::int64_t, k6_reduce_min,                                                       \
    (const std::int64_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_i64, std::int64_t, k6_reduce_max,                                                       \
    (const std::int64_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_i64, std::int64_t, k6_reduce_sum_wrap,                                                 \
    (const std::int64_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_i64, MinMaxSummary<std::int64_t>, k6_compute_sma,                                       \
    (const std::int64_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_u8, std::uint8_t, k6_reduce_min,                                                        \
    (const std::uint8_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_u8, std::uint8_t, k6_reduce_max,                                                        \
    (const std::uint8_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_u8, std::uint64_t, k6_reduce_sum_wrap,                                                 \
    (const std::uint8_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_u8, MinMaxSummary<std::uint8_t>, k6_compute_sma,                                        \
    (const std::uint8_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_u16, std::uint16_t, k6_reduce_min,                                                      \
    (const std::uint16_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_u16, std::uint16_t, k6_reduce_max,                                                      \
    (const std::uint16_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_u16, std::uint64_t, k6_reduce_sum_wrap,                                                \
    (const std::uint16_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_u16, MinMaxSummary<std::uint16_t>, k6_compute_sma,                                      \
    (const std::uint16_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_u32, std::uint32_t, k6_reduce_min,                                                      \
    (const std::uint32_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_u32, std::uint32_t, k6_reduce_max,                                                      \
    (const std::uint32_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_u32, std::uint64_t, k6_reduce_sum_wrap,                                                \
    (const std::uint32_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_u32, MinMaxSummary<std::uint32_t>, k6_compute_sma,                                      \
    (const std::uint32_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_u64, std::uint64_t, k6_reduce_min,                                                      \
    (const std::uint64_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_u64, std::uint64_t, k6_reduce_max,                                                      \
    (const std::uint64_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_u64, std::uint64_t, k6_reduce_sum_wrap,                                                \
    (const std::uint64_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_u64, MinMaxSummary<std::uint64_t>, k6_compute_sma,                                      \
    (const std::uint64_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len),                                              \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_f32, float, k6_reduce_min,                                                              \
    (const float* in, std::int64_t n, const std::uint8_t* validity, const std::uint32_t* sel,      \
     std::int64_t sel_len),                                                                        \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_f32, float, k6_reduce_max,                                                              \
    (const float* in, std::int64_t n, const std::uint8_t* validity, const std::uint32_t* sel,      \
     std::int64_t sel_len),                                                                        \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_f32, float, k6_reduce_sum_wrap,                                                        \
    (const float* in, std::int64_t n, const std::uint8_t* validity, const std::uint32_t* sel,      \
     std::int64_t sel_len),                                                                        \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_f32, MinMaxSummary<float>, k6_compute_sma,                                              \
    (const float* in, std::int64_t n, const std::uint8_t* validity, const std::uint32_t* sel,      \
     std::int64_t sel_len),                                                                        \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_min_f64, double, k6_reduce_min,                                                             \
    (const double* in, std::int64_t n, const std::uint8_t* validity, const std::uint32_t* sel,     \
     std::int64_t sel_len),                                                                        \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_max_f64, double, k6_reduce_max,                                                             \
    (const double* in, std::int64_t n, const std::uint8_t* validity, const std::uint32_t* sel,     \
     std::int64_t sel_len),                                                                        \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumw_f64, double, k6_reduce_sum_wrap,                                                       \
    (const double* in, std::int64_t n, const std::uint8_t* validity, const std::uint32_t* sel,     \
     std::int64_t sel_len),                                                                        \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sma_f64, MinMaxSummary<double>, k6_compute_sma,                                             \
    (const double* in, std::int64_t n, const std::uint8_t* validity, const std::uint32_t* sel,     \
     std::int64_t sel_len),                                                                        \
    (in, n, validity, sel, sel_len))                                                               \
  X(k6_sumc_i8, bool, k6_reduce_sum_checked,                                                       \
    (const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,                          \
     const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out_sum),                       \
    (in, n, validity, sel, sel_len, out_sum))                                                      \
  X(k6_sumc_i16, bool, k6_reduce_sum_checked,                                                      \
    (const std::int16_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out_sum),                       \
    (in, n, validity, sel, sel_len, out_sum))                                                      \
  X(k6_sumc_i32, bool, k6_reduce_sum_checked,                                                      \
    (const std::int32_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out_sum),                       \
    (in, n, validity, sel, sel_len, out_sum))                                                      \
  X(k6_sumc_i64, bool, k6_reduce_sum_checked,                                                      \
    (const std::int64_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out_sum),                       \
    (in, n, validity, sel, sel_len, out_sum))                                                      \
  X(k6_sumc_u8, bool, k6_reduce_sum_checked,                                                       \
    (const std::uint8_t* in, std::int64_t n, const std::uint8_t* validity,                         \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out_sum),                      \
    (in, n, validity, sel, sel_len, out_sum))                                                      \
  X(k6_sumc_u16, bool, k6_reduce_sum_checked,                                                      \
    (const std::uint16_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out_sum),                      \
    (in, n, validity, sel, sel_len, out_sum))                                                      \
  X(k6_sumc_u32, bool, k6_reduce_sum_checked,                                                      \
    (const std::uint32_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out_sum),                      \
    (in, n, validity, sel, sel_len, out_sum))                                                      \
  X(k6_sumc_u64, bool, k6_reduce_sum_checked,                                                      \
    (const std::uint64_t* in, std::int64_t n, const std::uint8_t* validity,                        \
     const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out_sum),                      \
    (in, n, validity, sel, sel_len, out_sum))                                                      \
  X(k7_hash_i8, void, k7_hash64,                                                                   \
    (const std::int8_t* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),               \
    (in, n, seed, out))                                                                            \
  X(k7_hash_i16, void, k7_hash64,                                                                  \
    (const std::int16_t* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),              \
    (in, n, seed, out))                                                                            \
  X(k7_hash_i32, void, k7_hash64,                                                                  \
    (const std::int32_t* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),              \
    (in, n, seed, out))                                                                            \
  X(k7_hash_i64, void, k7_hash64,                                                                  \
    (const std::int64_t* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),              \
    (in, n, seed, out))                                                                            \
  X(k7_hash_u8, void, k7_hash64,                                                                   \
    (const std::uint8_t* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),              \
    (in, n, seed, out))                                                                            \
  X(k7_hash_u16, void, k7_hash64,                                                                  \
    (const std::uint16_t* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),             \
    (in, n, seed, out))                                                                            \
  X(k7_hash_u32, void, k7_hash64,                                                                  \
    (const std::uint32_t* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),             \
    (in, n, seed, out))                                                                            \
  X(k7_hash_u64, void, k7_hash64,                                                                  \
    (const std::uint64_t* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),             \
    (in, n, seed, out))                                                                            \
  X(k7_hash_f32, void, k7_hash64,                                                                  \
    (const float* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out), (in, n, seed, out)) \
  X(k7_hash_f64, void, k7_hash64,                                                                  \
    (const double* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out),                    \
    (in, n, seed, out))                                                                            \
  X(k7_comb, void, k7_hash64_combine,                                                              \
    (const std::uint64_t* a, const std::uint64_t* b, std::int64_t n, std::uint64_t* out),          \
    (a, b, n, out))                                                                                \
  X(k8_unpack_u8, void, k8_unpack,                                                                 \
    (const std::uint8_t* packed, std::int64_t n, int bit_width, std::uint8_t base,                 \
     std::uint8_t* out),                                                                           \
    (packed, n, bit_width, base, out))                                                             \
  X(k8_unpack_u16, void, k8_unpack,                                                                \
    (const std::uint8_t* packed, std::int64_t n, int bit_width, std::uint16_t base,                \
     std::uint16_t* out),                                                                          \
    (packed, n, bit_width, base, out))                                                             \
  X(k8_unpack_u32, void, k8_unpack,                                                                \
    (const std::uint8_t* packed, std::int64_t n, int bit_width, std::uint32_t base,                \
     std::uint32_t* out),                                                                          \
    (packed, n, bit_width, base, out))                                                             \
  X(k8_unpack_u64, void, k8_unpack,                                                                \
    (const std::uint8_t* packed, std::int64_t n, int bit_width, std::uint64_t base,                \
     std::uint64_t* out),                                                                          \
    (packed, n, bit_width, base, out))                                                             \
  X(k9_arith_i8, void, k9_arith,                                                                   \
    (ArithOp op, const std::int8_t* a, const std::int8_t* b, std::int64_t n, std::int8_t* out),    \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_i8, void, k9_arith_scalar_rhs,                                                       \
    (ArithOp op, const std::int8_t* a, std::int8_t b, std::int64_t n, std::int8_t* out),           \
    (op, a, b, n, out))                                                                            \
  X(k9_arith_i16, void, k9_arith,                                                                  \
    (ArithOp op, const std::int16_t* a, const std::int16_t* b, std::int64_t n, std::int16_t* out), \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_i16, void, k9_arith_scalar_rhs,                                                      \
    (ArithOp op, const std::int16_t* a, std::int16_t b, std::int64_t n, std::int16_t* out),        \
    (op, a, b, n, out))                                                                            \
  X(k9_arith_i32, void, k9_arith,                                                                  \
    (ArithOp op, const std::int32_t* a, const std::int32_t* b, std::int64_t n, std::int32_t* out), \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_i32, void, k9_arith_scalar_rhs,                                                      \
    (ArithOp op, const std::int32_t* a, std::int32_t b, std::int64_t n, std::int32_t* out),        \
    (op, a, b, n, out))                                                                            \
  X(k9_arith_i64, void, k9_arith,                                                                  \
    (ArithOp op, const std::int64_t* a, const std::int64_t* b, std::int64_t n, std::int64_t* out), \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_i64, void, k9_arith_scalar_rhs,                                                      \
    (ArithOp op, const std::int64_t* a, std::int64_t b, std::int64_t n, std::int64_t* out),        \
    (op, a, b, n, out))                                                                            \
  X(k9_arith_u8, void, k9_arith,                                                                   \
    (ArithOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n, std::uint8_t* out), \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_u8, void, k9_arith_scalar_rhs,                                                       \
    (ArithOp op, const std::uint8_t* a, std::uint8_t b, std::int64_t n, std::uint8_t* out),        \
    (op, a, b, n, out))                                                                            \
  X(k9_arith_u16, void, k9_arith,                                                                  \
    (ArithOp op, const std::uint16_t* a, const std::uint16_t* b, std::int64_t n,                   \
     std::uint16_t* out),                                                                          \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_u16, void, k9_arith_scalar_rhs,                                                      \
    (ArithOp op, const std::uint16_t* a, std::uint16_t b, std::int64_t n, std::uint16_t* out),     \
    (op, a, b, n, out))                                                                            \
  X(k9_arith_u32, void, k9_arith,                                                                  \
    (ArithOp op, const std::uint32_t* a, const std::uint32_t* b, std::int64_t n,                   \
     std::uint32_t* out),                                                                          \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_u32, void, k9_arith_scalar_rhs,                                                      \
    (ArithOp op, const std::uint32_t* a, std::uint32_t b, std::int64_t n, std::uint32_t* out),     \
    (op, a, b, n, out))                                                                            \
  X(k9_arith_u64, void, k9_arith,                                                                  \
    (ArithOp op, const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,                   \
     std::uint64_t* out),                                                                          \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_u64, void, k9_arith_scalar_rhs,                                                      \
    (ArithOp op, const std::uint64_t* a, std::uint64_t b, std::int64_t n, std::uint64_t* out),     \
    (op, a, b, n, out))                                                                            \
  X(k9_arith_f32, void, k9_arith,                                                                  \
    (ArithOp op, const float* a, const float* b, std::int64_t n, float* out), (op, a, b, n, out))  \
  X(k9_ariths_f32, void, k9_arith_scalar_rhs,                                                      \
    (ArithOp op, const float* a, float b, std::int64_t n, float* out), (op, a, b, n, out))         \
  X(k9_arith_f64, void, k9_arith,                                                                  \
    (ArithOp op, const double* a, const double* b, std::int64_t n, double* out),                   \
    (op, a, b, n, out))                                                                            \
  X(k9_ariths_f64, void, k9_arith_scalar_rhs,                                                      \
    (ArithOp op, const double* a, double b, std::int64_t n, double* out), (op, a, b, n, out))      \
  X(k10_chk_i8, std::int64_t, k10_arith_checked,                                                   \
    (ArithOp op, const std::int8_t* a, const std::int8_t* b, std::int64_t n, std::int8_t* out,     \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_chks_i8, std::int64_t, k10_arith_checked_scalar_rhs,                                       \
    (ArithOp op, const std::int8_t* a, std::int8_t b, std::int64_t n, std::int8_t* out,            \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_sat_i8, void, k10_arith_saturating,                                                        \
    (ArithOp op, const std::int8_t* a, const std::int8_t* b, std::int64_t n, std::int8_t* out),    \
    (op, a, b, n, out))                                                                            \
  X(k10_sats_i8, void, k10_arith_saturating_scalar_rhs,                                            \
    (ArithOp op, const std::int8_t* a, std::int8_t b, std::int64_t n, std::int8_t* out),           \
    (op, a, b, n, out))                                                                            \
  X(k10_chk_i16, std::int64_t, k10_arith_checked,                                                  \
    (ArithOp op, const std::int16_t* a, const std::int16_t* b, std::int64_t n, std::int16_t* out,  \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_chks_i16, std::int64_t, k10_arith_checked_scalar_rhs,                                      \
    (ArithOp op, const std::int16_t* a, std::int16_t b, std::int64_t n, std::int16_t* out,         \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_sat_i16, void, k10_arith_saturating,                                                       \
    (ArithOp op, const std::int16_t* a, const std::int16_t* b, std::int64_t n, std::int16_t* out), \
    (op, a, b, n, out))                                                                            \
  X(k10_sats_i16, void, k10_arith_saturating_scalar_rhs,                                           \
    (ArithOp op, const std::int16_t* a, std::int16_t b, std::int64_t n, std::int16_t* out),        \
    (op, a, b, n, out))                                                                            \
  X(k10_chk_i32, std::int64_t, k10_arith_checked,                                                  \
    (ArithOp op, const std::int32_t* a, const std::int32_t* b, std::int64_t n, std::int32_t* out,  \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_chks_i32, std::int64_t, k10_arith_checked_scalar_rhs,                                      \
    (ArithOp op, const std::int32_t* a, std::int32_t b, std::int64_t n, std::int32_t* out,         \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_sat_i32, void, k10_arith_saturating,                                                       \
    (ArithOp op, const std::int32_t* a, const std::int32_t* b, std::int64_t n, std::int32_t* out), \
    (op, a, b, n, out))                                                                            \
  X(k10_sats_i32, void, k10_arith_saturating_scalar_rhs,                                           \
    (ArithOp op, const std::int32_t* a, std::int32_t b, std::int64_t n, std::int32_t* out),        \
    (op, a, b, n, out))                                                                            \
  X(k10_chk_i64, std::int64_t, k10_arith_checked,                                                  \
    (ArithOp op, const std::int64_t* a, const std::int64_t* b, std::int64_t n, std::int64_t* out,  \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_chks_i64, std::int64_t, k10_arith_checked_scalar_rhs,                                      \
    (ArithOp op, const std::int64_t* a, std::int64_t b, std::int64_t n, std::int64_t* out,         \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_sat_i64, void, k10_arith_saturating,                                                       \
    (ArithOp op, const std::int64_t* a, const std::int64_t* b, std::int64_t n, std::int64_t* out), \
    (op, a, b, n, out))                                                                            \
  X(k10_sats_i64, void, k10_arith_saturating_scalar_rhs,                                           \
    (ArithOp op, const std::int64_t* a, std::int64_t b, std::int64_t n, std::int64_t* out),        \
    (op, a, b, n, out))                                                                            \
  X(k10_chk_u8, std::int64_t, k10_arith_checked,                                                   \
    (ArithOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n, std::uint8_t* out,  \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_chks_u8, std::int64_t, k10_arith_checked_scalar_rhs,                                       \
    (ArithOp op, const std::uint8_t* a, std::uint8_t b, std::int64_t n, std::uint8_t* out,         \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_sat_u8, void, k10_arith_saturating,                                                        \
    (ArithOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n, std::uint8_t* out), \
    (op, a, b, n, out))                                                                            \
  X(k10_sats_u8, void, k10_arith_saturating_scalar_rhs,                                            \
    (ArithOp op, const std::uint8_t* a, std::uint8_t b, std::int64_t n, std::uint8_t* out),        \
    (op, a, b, n, out))                                                                            \
  X(k10_chk_u16, std::int64_t, k10_arith_checked,                                                  \
    (ArithOp op, const std::uint16_t* a, const std::uint16_t* b, std::int64_t n,                   \
     std::uint16_t* out, std::uint8_t* overflow_bits),                                             \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_chks_u16, std::int64_t, k10_arith_checked_scalar_rhs,                                      \
    (ArithOp op, const std::uint16_t* a, std::uint16_t b, std::int64_t n, std::uint16_t* out,      \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_sat_u16, void, k10_arith_saturating,                                                       \
    (ArithOp op, const std::uint16_t* a, const std::uint16_t* b, std::int64_t n,                   \
     std::uint16_t* out),                                                                          \
    (op, a, b, n, out))                                                                            \
  X(k10_sats_u16, void, k10_arith_saturating_scalar_rhs,                                           \
    (ArithOp op, const std::uint16_t* a, std::uint16_t b, std::int64_t n, std::uint16_t* out),     \
    (op, a, b, n, out))                                                                            \
  X(k10_chk_u32, std::int64_t, k10_arith_checked,                                                  \
    (ArithOp op, const std::uint32_t* a, const std::uint32_t* b, std::int64_t n,                   \
     std::uint32_t* out, std::uint8_t* overflow_bits),                                             \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_chks_u32, std::int64_t, k10_arith_checked_scalar_rhs,                                      \
    (ArithOp op, const std::uint32_t* a, std::uint32_t b, std::int64_t n, std::uint32_t* out,      \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_sat_u32, void, k10_arith_saturating,                                                       \
    (ArithOp op, const std::uint32_t* a, const std::uint32_t* b, std::int64_t n,                   \
     std::uint32_t* out),                                                                          \
    (op, a, b, n, out))                                                                            \
  X(k10_sats_u32, void, k10_arith_saturating_scalar_rhs,                                           \
    (ArithOp op, const std::uint32_t* a, std::uint32_t b, std::int64_t n, std::uint32_t* out),     \
    (op, a, b, n, out))                                                                            \
  X(k10_chk_u64, std::int64_t, k10_arith_checked,                                                  \
    (ArithOp op, const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,                   \
     std::uint64_t* out, std::uint8_t* overflow_bits),                                             \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_chks_u64, std::int64_t, k10_arith_checked_scalar_rhs,                                      \
    (ArithOp op, const std::uint64_t* a, std::uint64_t b, std::int64_t n, std::uint64_t* out,      \
     std::uint8_t* overflow_bits),                                                                 \
    (op, a, b, n, out, overflow_bits))                                                             \
  X(k10_sat_u64, void, k10_arith_saturating,                                                       \
    (ArithOp op, const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,                   \
     std::uint64_t* out),                                                                          \
    (op, a, b, n, out))                                                                            \
  X(k10_sats_u64, void, k10_arith_saturating_scalar_rhs,                                           \
    (ArithOp op, const std::uint64_t* a, std::uint64_t b, std::int64_t n, std::uint64_t* out),     \
    (op, a, b, n, out))

QUIVER_BEGIN_NAMESPACE
namespace detail {

// Number of concrete kernel symbols in the inventory.
inline constexpr int kKernelEntryCount = 0
// X-macro summation idiom: `+1` terms concatenate onto the leading 0; parenthesizing
// would break the expression.
#define QUIVER_COUNT_ENTRY(uid, ret, name, params, args) +1  // NOLINT(bugprone-macro-parentheses)
    QUIVER_KERNEL_ENTRY_LIST(QUIVER_COUNT_ENTRY)
#undef QUIVER_COUNT_ENTRY
    ;

// Dispatched-wrapper overload declarations — the symbols the public facades call.
#define QUIVER_DECLARE_ENTRY(uid, ret, name, params, args) ret name params noexcept;
QUIVER_KERNEL_ENTRY_LIST(QUIVER_DECLARE_ENTRY)
#undef QUIVER_DECLARE_ENTRY

}  // namespace detail
QUIVER_END_NAMESPACE
