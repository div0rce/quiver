# Compatibility

Three different claims, kept deliberately distinct:

- **compiles** — the platform builds Quiver;
- **correctness-tested** — the full suite (unit, property, differential across ISA tiers,
  invariant, guard-page, sanitizers) runs and passes there, in CI or under an emulator;
- **performance-measured** — committed ledger entries exist from a registered machine.

## Platform matrix

| Platform | Scalar | AVX2 | AVX-512 | NEON | Performance evidence |
|---|---|---|---|---|---|
| Linux x86-64 (GCC 13/14, Clang 17/19) | tested | tested | tested under Intel SDE¹ | — | pending native machines |
| macOS ARM64 (Apple Clang) | tested | — | — | tested | **Apple M2 ledger** |
| Linux ARM64 (GCC 14, Clang 19) | tested | — | — | tested | pending |
| Windows x86-64 (MSVC) | tier-2² | tier-2² | tier-2² | — | pending |

¹ There is no AVX-512 hardware in CI; correctness runs under the Intel Software Development
Emulator on the `-spr` and `-skx` profiles every push (ADR-010). No AVX-512 performance number is
published anywhere — that is exactly the missing-machine gap ([help here](https://github.com/div0rce/quiver/issues/39)).

² MSVC is a best-effort toolchain tier: the amalgamation builds and passes the unit suite in CI
with the default `/arch`, with two documented exclusions (a checked-sum fallback without
`__int128`, and the Windows guard-page harness — R-18). Tier-1 promises are made only for
GCC and Clang.

## Toolchain floor

- **C++23**, CMake ≥ 3.28. CI pins GCC 13/14 and Clang 17/19 as the tier-1 matrix.
- The amalgamation drop-in compiles as one ordinary translation unit — exceptions on or off
  (`-fno-exceptions` consumer is CI-verified), RTTI on or off.

## Result identity across CPUs

Integer and hash results are bit-identical across every ISA tier — this is enforced by a
differential test matrix on every push, not promised. Floating-point sums follow one documented
reassociation policy (ADR-013): a result is reproducible for a given version, ISA tier, and
build, and the scalar tier is the strict-order reference.

## What "pending" means here

Pending rows are not weasel words for "probably fine": they mean no registered machine has
produced committed ledger entries there yet. The [hardware coverage plan](benchmarks/hardware-coverage-plan.md)
tracks exactly which machine classes are missing, and a
[hardware benchmark submission](https://github.com/div0rce/quiver/issues/38) from any of them is
the highest-value contribution the project can receive.
