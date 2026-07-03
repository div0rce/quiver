#!/usr/bin/env bash
# Flamegraph collection wrapper (REQ-BENCH-014): perf record around a bench invocation,
# collapsed stacks emitted for investigation artifacts under
# docs/benchmarks/investigations/<topic>/ (question + environment + conclusion required).
# Flamegraphs accompany investigations; they never replace ledger numbers (Survey §7.2).
# Linux-only (perf); usage: bench/harness/flamegraph.sh <output-prefix> <bench-binary> [args…]
set -euo pipefail

if [ "$(uname -s)" != "Linux" ]; then
  echo "flamegraph.sh: requires Linux perf (REQ-BENCH-014); macOS profiling goes through" >&2
  echo "Instruments and is out of scope for the committed workflow (Survey §7.3)." >&2
  exit 2
fi
if [ $# -lt 2 ]; then
  echo "usage: $0 <output-prefix> <bench-binary> [bench args...]" >&2
  exit 2
fi

out_prefix=$1
shift

perf record -g --output="${out_prefix}.perf.data" -- "$@"
perf script --input="${out_prefix}.perf.data" \
  | awk '/^\S/ {comm=$1} /^\s/ {gsub(/^\s+/,""); stack = $1 ";" stack} /^$/ {if (stack) print comm ";" stack " 1"; stack=""}' \
  > "${out_prefix}.collapsed"

echo "flamegraph.sh: wrote ${out_prefix}.perf.data and ${out_prefix}.collapsed"
echo "Render with any FlameGraph-compatible tool; commit the collapsed stacks plus the"
echo "question/environment/conclusion note to docs/benchmarks/investigations/<topic>/."
