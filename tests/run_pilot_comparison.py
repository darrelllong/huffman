#!/usr/bin/env python3
"""Run Pilot benchmarks for C and Rust Huffman tools on Gutenberg workloads."""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    p = argparse.ArgumentParser()
    p.add_argument(
        "--bench",
        default=str(Path.home() / "pilot-bench" / "build" / "cli" / "bench"),
        help="Path to Pilot bench executable",
    )
    p.add_argument(
        "--out-dir",
        default=str(root / "workloads" / "pilot_runs"),
        help="Directory to store Pilot outputs",
    )
    p.add_argument("--preset", default="quick", choices=("quick", "normal", "strict"))
    p.add_argument("--session-limit", type=int, default=120, help="Seconds per case")
    return p.parse_args()


def read_pi_results(path: Path) -> tuple[float, float, int]:
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        row = next(reader)
        mean = float(row["readings_mean"])
        ci = float(row["readings_subsession_ci"])
        n = int(row["readings_num"])
        return mean, ci, n


def main() -> int:
    args = parse_args()
    bench = Path(args.bench).resolve()
    if not bench.exists():
        print(f"missing bench executable: {bench}", file=sys.stderr)
        return 2

    root = Path(__file__).resolve().parents[1]
    runner = root / "tests" / "run_huffman_case.py"
    workloads = root / "workloads" / "gutenberg"
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    c_encode = root / "encode"
    c_decode = root / "decode"
    rust_encode = root / "rust" / "target" / "release" / "encode"
    rust_decode = root / "rust" / "target" / "release" / "decode"
    for exe in (c_encode, c_decode, rust_encode, rust_decode, runner):
        if not exe.exists():
            print(f"missing executable: {exe}", file=sys.stderr)
            return 2

    cases = [
        ("c_encode_shakespeare", c_encode, "encode", workloads / "shakespeare_complete_works_100-0.txt"),
        ("rust_encode_shakespeare", rust_encode, "encode", workloads / "shakespeare_complete_works_100-0.txt"),
        ("c_decode_shakespeare", c_decode, "decode", workloads / "shakespeare_complete_works_100-0.txt.huf"),
        ("rust_decode_shakespeare", rust_decode, "decode", workloads / "shakespeare_complete_works_100-0.txt.huf"),
        ("c_encode_kipling", c_encode, "encode", workloads / "kipling_collected_workload.txt"),
        ("rust_encode_kipling", rust_encode, "encode", workloads / "kipling_collected_workload.txt"),
        ("c_decode_kipling", c_decode, "decode", workloads / "kipling_collected_workload.txt.huf"),
        ("rust_decode_kipling", rust_decode, "decode", workloads / "kipling_collected_workload.txt.huf"),
    ]

    results: list[tuple[str, str, str, float, float, int]] = []
    for case_name, exe, mode, inp in cases:
        case_dir = out_dir / case_name
        case_dir.mkdir(parents=True, exist_ok=True)
        cmd = [
            str(bench),
            "run_program",
            "--preset",
            args.preset,
            "--session-limit",
            str(args.session_limit),
            "-q",
            "-p",
            "duration,s,0,0,1",
            "-o",
            str(case_dir),
            "--",
            "python3",
            str(runner),
            "--exe",
            str(exe),
            "--mode",
            mode,
            "--input",
            str(inp),
        ]
        print(f"[pilot] {case_name}")
        proc = subprocess.run(cmd, check=False)
        if proc.returncode != 0:
            print(f"[pilot] case failed: {case_name} rc={proc.returncode}", file=sys.stderr)
            return proc.returncode
        mean, ci, n = read_pi_results(case_dir / "pi_results.csv")
        impl = "c" if case_name.startswith("c_") else "rust"
        workload = "shakespeare" if "shakespeare" in case_name else "kipling"
        results.append((case_name, impl, mode, mean, ci, n))

    summary_path = out_dir / "comparison_summary.csv"
    with summary_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["case", "impl", "mode", "mean_sec", "ci_sec", "readings"])
        writer.writerows(results)

    print("\ncase,impl,mode,mean_sec,ci_sec,readings")
    for row in results:
        print(",".join([row[0], row[1], row[2], f"{row[3]:.9f}", f"{row[4]:.9f}", str(row[5])]))
    print(f"\nsummary: {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
