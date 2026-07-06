# Quiver

**A dependency-free C++23 library of vectorized analytical kernels, with a reproducible performance ledger.**

Quiver implements the ~10 kernel families every analytical engine reimplements — predicate
evaluation, filter/compaction, selection↔bitmap conversion, mask algebra, gather/dictionary
decode, reductions/SMA, batch hashing, bit-unpacking, and overflow-guarded arithmetic —
hand-implemented per ISA with runtime dispatch, and shipped alongside a reproducible,
methodology-first performance ledger. It is infrastructure you import: not a database, SQL engine,
storage engine, scheduler, or general SIMD abstraction. Product boundaries are fixed by the
[Design Charter](docs/design/DESIGN_CHARTER.md); the engineering spec by the
[Engineering PRD](docs/prd/README.md).

## Project status — v0.6.0 (2026-07-05)

Milestones **M0–M8 are complete and released** (v0.1.0 → v0.6.0). **M9 and M10 are partial /
deferred** on benchmark hardware, not failed — see the deferrals below and the
[final implementation report](docs/releases/final-implementation-report.md), which is the source of
truth for status. Gate evidence per milestone lives in [docs/releases/gates/](docs/releases/gates/).

| Area | State |
|------|-------|
| **Kernels** | All ten families live: **K1** compare · **K2** filter · **K3** select · **K4** mask · **K5** take/dict-decode · **K6** reduce/SMA · **K7** hash · **K8** unpack · **K9** arith · **K10** guarded arith |
| **ISAs** | **scalar, AVX2, NEON** natively implemented and tested; **AVX-512** correctness validated in CI under **Intel SDE** (`-spr` and `-skx`), with documented AVX2 fall-throughs for families where a distinct AVX-512 technique doesn't win (K5/K6/K10) and VBMI2 8/16-bit compaction deferred |
| **Consumption** | Installable CMake package (`find_package(Quiver CONFIG)`), source subproject (`FetchContent`/`add_subdirectory`), and single-file **amalgamation drop-in** (`quiver.h` + `quiver.cpp`) — all three CI-verified, including a `-fno-exceptions` consumer. Compile-time ISA pinning via `QUIVER_PIN_ISA`. See the [vendoring guide](docs/guides/vendoring.md) |
| **Ledger** | Real measurements exist on **one** registered machine — **Apple M2** (a secondary, no-PMU platform). Multi-microarchitecture coverage (the REQ-LEDGER-012 ≥5-µarch gate) and the AVX-512 performance ledger are **deferred under R-06** pending registered x86 / additional ARM hardware. No cross-µarch numbers are invented in the interim (Charter T7) — see the [hardware coverage plan](docs/benchmarks/hardware-coverage-plan.md) |

### Deferrals (honest, hardware/infra-gated — none a correctness gap)

- **R-06** — only Apple M2 is registered; the multi-µarch ledger, the M9 representation study's
  cross-µarch conclusion, and the M10 v1.0 coverage/regression gates all wait on more hardware.
- **R-19** — the nightly's MSan-instrumented-libc++ leg has never passed (pre-existing infra), so
  `release.yml` correctly **withholds the v0.6.0 draft** (a green nightly is a release
  precondition). The workflow is authored and statically validated; the v0.6.0 tag has **no
  attached artifacts yet** — generate the amalgamation locally with
  `python3 tools/amalgamate/amalgamate.py --out-dir <dir>` in the meantime.
- **v1.0** — the API-freeze audit (signatures ↔ headers ↔ docs, traceability, ADR status,
  zero disabled tests) is **clean**; v1.0 certification and tagging are deferred pending the
  hardware coverage/regression gates above. R-17/R-18 record tier-2 MSVC amalgamation caveats.

## Quick start

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
```

```cpp
#include "quiver/quiver.h"
// compare (K1) → filter (K2): keep elements > 4
int v[] = {5, 1, 9, 3, 7}; std::uint8_t bits[1] = {0}; int out[5];
quiver::compare_bitmap(quiver::CompareOp::kGt, quiver::BatchView<int>{v, 5}, 4,
                       quiver::BitmapView{nullptr}, bits);
auto kept = quiver::filter(quiver::BatchView<int>{v, 5}, quiver::BitmapView{bits}, out);
```

Runnable examples: [`examples/`](examples/).

## Documentation

| Document | Purpose |
|---|---|
| [Final implementation report](docs/releases/final-implementation-report.md) | Current status of every milestone (source of truth) |
| [Design Charter](docs/design/DESIGN_CHARTER.md) | Product definition: mission, tenets, scope, contracts |
| [Engineering PRD](docs/prd/README.md) | Complete engineering specification (requirements, ADRs, milestones) |
| [Architecture Decision Records](docs/adr/README.md) | The 26 settled engineering decisions |
| [Vendoring guide](docs/guides/vendoring.md) | The three consumption modes |
| [Building](docs/guides/building.md) | Configure/build instructions |
| [Performance ledger](ledger/README.md) | The ledger's data model, coverage, and how to read numbers |
| [Contributing](CONTRIBUTING.md) | Workflow, DCO, review standards |

## License

[Apache-2.0](LICENSE).
