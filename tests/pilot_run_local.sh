#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BENCH_DEFAULT="${HOME}/pilot-bench/build/cli/bench"

BENCH="${PILOT_BENCH:-${BENCH_DEFAULT}}"
PRESET="${PRESET:-quick}"
SESSION_LIMIT="${SESSION_LIMIT:-600}"
OUT_DIR="${OUT_DIR:-${ROOT}/workloads/pilot_runs}"

python3 "${ROOT}/tests/run_pilot_comparison.py" \
  --bench "${BENCH}" \
  --preset "${PRESET}" \
  --session-limit "${SESSION_LIMIT}" \
  --out-dir "${OUT_DIR}"
