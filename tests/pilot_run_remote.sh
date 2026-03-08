#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

REMOTE_HOST="${REMOTE_HOST:-}"
REMOTE_REPO="${REMOTE_REPO:-huffman}"
PRESET="${PRESET:-quick}"
SESSION_LIMIT="${SESSION_LIMIT:-600}"
REMOTE_OUT_DIR="${REMOTE_OUT_DIR:-workloads/pilot_runs_remote}"
SYNC="${SYNC:-1}"
PULL_SUMMARY="${PULL_SUMMARY:-1}"

if [[ -z "${REMOTE_HOST}" ]]; then
  echo "error: set REMOTE_HOST (example: REMOTE_HOST=benchbox.lan)" >&2
  exit 2
fi

if [[ "${SYNC}" == "1" ]]; then
  rsync -az --delete --exclude '.git/' "${ROOT}/" "${REMOTE_HOST}:${REMOTE_REPO}/"
fi

ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && make && cd rust && cargo build --release && cd .. && \
python3 tests/run_pilot_comparison.py --preset '${PRESET}' --session-limit '${SESSION_LIMIT}' --out-dir '${REMOTE_OUT_DIR}'"

if [[ "${PULL_SUMMARY}" == "1" ]]; then
  mkdir -p "${ROOT}/workloads/pilot_runs_remote"
  scp "${REMOTE_HOST}:${REMOTE_REPO}/${REMOTE_OUT_DIR}/comparison_summary.csv" \
      "${ROOT}/workloads/pilot_runs_remote/comparison_summary.csv"
fi
