#!/usr/bin/env python3
"""Run Pilot benchmarks for C and Rust Huffman tools on selected workloads."""

from __future__ import annotations

import argparse
import csv
import glob
import re
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


def read_ci_level_percent(session_log: Path) -> int | None:
    if not session_log.exists():
        return None
    pattern = re.compile(r"T score for (\d+)% confidence level")
    with session_log.open(encoding="utf-8", errors="replace") as f:
        for line in f:
            match = pattern.search(line)
            if match:
                return int(match.group(1))
    return None


def main() -> int:
    args = parse_args()
    bench = Path(args.bench).resolve()
    if not bench.exists():
        print(f"missing bench executable: {bench}", file=sys.stderr)
        return 2

    root = Path(__file__).resolve().parents[1]
    runner = root / "tests" / "run_huffman_case.py"
    gutenberg = root / "workloads" / "gutenberg"
    kernel_dir = root / "workloads" / "kernel"
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

    workloads: list[tuple[str, Path]] = [
        ("shakespeare", gutenberg / "shakespeare_complete_works_100-0.txt"),
        ("kipling", gutenberg / "kipling_collected_workload.txt"),
    ]
    kernel_candidates = sorted(glob.glob(str(kernel_dir / "linux-*.tar.xz")))
    if kernel_candidates:
        workloads.append(("kernel", Path(kernel_candidates[-1])))

    for workload_name, plain in workloads:
        if not plain.exists():
            print(f"missing workload input: {plain}", file=sys.stderr)
            return 2
        encoded = plain.with_name(plain.name + ".huf")
        if not encoded.exists():
            print(f"[prep] generating {encoded.name} for decode cases")
            rc = subprocess.run(
                [str(c_encode), "-i", str(plain), "-o", str(encoded)],
                check=False,
            ).returncode
            if rc != 0:
                print(f"failed to generate decode input: {encoded}", file=sys.stderr)
                return rc

    cases: list[tuple[str, Path, str, Path]] = []
    for workload_name, plain in workloads:
        encoded = plain.with_name(plain.name + ".huf")
        cases.extend([
            (f"c_encode_{workload_name}", c_encode, "encode", plain),
            (f"rust_encode_{workload_name}", rust_encode, "encode", plain),
            (f"c_decode_{workload_name}", c_decode, "decode", encoded),
            (f"rust_decode_{workload_name}", rust_decode, "decode", encoded),
        ])

    results: list[tuple[str, str, str, str, float, float, int | None, int]] = []
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
        ci_level = read_ci_level_percent(case_dir / "session_log.txt")
        try:
            impl, operation, workload = case_name.split("_", 2)
        except ValueError:
            print(f"unexpected case name: {case_name}", file=sys.stderr)
            return 2
        results.append((case_name, impl, operation, workload, mean, ci, ci_level, n))

    summary_path = out_dir / "comparison_summary.csv"
    with summary_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "case",
            "impl",
            "operation",
            "workload",
            "mean_seconds",
            "ci_width_seconds",
            "ci_level_percent",
            "repetitions",
        ])
        writer.writerows(results)

    print("\ncase,impl,operation,workload,mean_seconds,ci_width_seconds,ci_level_percent,repetitions")
    for row in results:
        ci_level_str = "" if row[6] is None else str(row[6])
        print(",".join([
            row[0],
            row[1],
            row[2],
            row[3],
            f"{row[4]:.9f}",
            f"{row[5]:.9f}",
            ci_level_str,
            str(row[7]),
        ]))
    print(f"\nsummary: {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
