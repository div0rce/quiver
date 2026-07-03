// First-party bounded fuzz decoder + differential-fuzzing helpers (REQ-TEST-007, ADR-009).
// Harnesses decode fuzz bytes into CONTRACT-VALID parameters only (ADR-025: selection
// vectors sorted-unique in range, indices/codes in bounds; UB-class violations are prevented
// by construction, never exercised) and then run the same call on every host-available
// backend, asserting equality — bit-exact except where the defined-output contract or the
// documented float-sum policy says otherwise (REQ-MEM-008 defined regions; ADR-013).
// Exhausted input decodes as zeroes, so every byte stream is a valid test case.
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-SEC-001 | ADR-009, ADR-025
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "quiver/dispatch.h"

namespace quiver_fuzz {

[[noreturn]] inline void fail(const char* what) {
  std::fprintf(stderr, "fuzz differential failure: %s\n", what);
  std::abort();
}

inline void check(bool ok, const char* what) {
  if (!ok) {
    fail(what);
  }
}

class Decoder {
 public:
  Decoder(const std::uint8_t* data, std::size_t size) : p_(data), left_(size) {}

  std::uint8_t u8() {
    std::uint8_t v = 0;
    fill_bytes(&v, 1);
    return v;
  }

  std::uint32_t u32() {
    std::uint32_t v = 0;
    fill_bytes(&v, 4);
    return v;
  }

  // Uniform-ish pick in [0, n).
  int pick(int n) { return static_cast<int>(u8() % static_cast<unsigned>(n)); }

  // Length in [0, max].
  std::int64_t len(std::int64_t max) {
    return static_cast<std::int64_t>(u32() % static_cast<std::uint32_t>(max + 1));
  }

  bool boolean() { return (u8() & 1u) != 0; }

  template <class T>
  void fill(T* dst, std::int64_t n) {
    fill_bytes(dst, static_cast<std::size_t>(n) * sizeof(T));
  }

  template <class T>
  T value() {
    T v{};
    fill_bytes(&v, sizeof(T));
    return v;
  }

 private:
  void fill_bytes(void* dst, std::size_t want) {
    const std::size_t got = want < left_ ? want : left_;
    std::memcpy(dst, p_, got);
    std::memset(static_cast<std::uint8_t*>(dst) + got, 0, want - got);
    p_ += got;
    left_ -= got;
  }

  const std::uint8_t* p_;
  std::size_t left_;
};

// Bitmap over n elements with arbitrary bit content (inputs need not be tail-clean).
inline std::vector<std::uint8_t> bitmap(Decoder& d, std::int64_t n) {
  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8) + 1);
  d.fill(bits.data(), (n + 7) / 8);
  return bits;
}

// Sorted-unique selection vector over [0, n) — contract-valid by construction (ADR-025).
inline std::vector<std::uint32_t> selvec(Decoder& d, std::int64_t n) {
  const std::vector<std::uint8_t> bits = bitmap(d, n);
  std::vector<std::uint32_t> out;
  for (std::int64_t i = 0; i < n; ++i) {
    if (((bits[static_cast<std::size_t>(i >> 3)] >> (i & 7)) & 1u) != 0) {
      out.push_back(static_cast<std::uint32_t>(i));
    }
  }
  return out;
}

// Host-available backends, scalar first (the comparison baseline).
inline std::vector<quiver::Isa> host_backends() {
  std::vector<quiver::Isa> v{quiver::Isa::kScalar};
  for (const quiver::Isa isa : {quiver::Isa::kNeon, quiver::Isa::kAvx2, quiver::Isa::kAvx512}) {
    if (quiver::cpu_supports(isa)) {
      v.push_back(isa);
    }
  }
  return v;
}

// Applies f to a zero of the element type selected by the fuzz stream (REQ-API-004 set).
template <class F>
void with_element_type(Decoder& d, F f) {
  switch (d.pick(10)) {
  case 0:
    f(std::int8_t{});
    break;
  case 1:
    f(std::int16_t{});
    break;
  case 2:
    f(std::int32_t{});
    break;
  case 3:
    f(std::int64_t{});
    break;
  case 4:
    f(std::uint8_t{});
    break;
  case 5:
    f(std::uint16_t{});
    break;
  case 6:
    f(std::uint32_t{});
    break;
  case 7:
    f(std::uint64_t{});
    break;
  case 8:
    f(float{});
    break;
  default:
    f(double{});
    break;
  }
}

}  // namespace quiver_fuzz
