#!/usr/bin/env python3
"""Run one Huffman encode/decode case and print duration in seconds.

Output format is CSV with one column:
  <seconds>
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True, help="Path to encode/decode executable")
    p.add_argument("--mode", required=True, choices=("encode", "decode"))
    p.add_argument("--input", required=True, help="Input file path")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    exe = Path(args.exe)
    inp = Path(args.input)
    if not exe.exists():
        print(f"missing executable: {exe}", file=sys.stderr)
        return 2
    if not inp.exists():
        print(f"missing input: {inp}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="huff_pilot_") as td:
        out = Path(td) / ("out.huf" if args.mode == "encode" else "out.bin")
        cmd = [str(exe), "-i", str(inp), "-o", str(out)]
        t0 = time.perf_counter()
        proc = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        elapsed = time.perf_counter() - t0
        if proc.returncode != 0:
            return proc.returncode

        # Ensure output exists and is non-empty when input is non-empty.
        if not out.exists():
            print("output missing", file=sys.stderr)
            return 3
        if inp.stat().st_size > 0 and out.stat().st_size == 0:
            print("unexpected empty output", file=sys.stderr)
            return 4

        print(f"{elapsed:.9f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
