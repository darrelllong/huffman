#!/usr/bin/env python3
"""Coverage-oriented black-box fuzzer for Huffman encode/decode binaries.

The fuzzer runs in two modes:
1) Round-trip mode: random plaintext -> encode -> decode -> compare bytes.
2) Mutation mode: mutate compressed byte streams and feed them to decode.

Any crash, timeout, sanitizer diagnostic, or round-trip mismatch is captured.
"""

from __future__ import annotations

import argparse
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Optional


SANITIZER_MARKERS = (
    "AddressSanitizer",
    "LeakSanitizer",
    "UndefinedBehaviorSanitizer",
    "runtime error:",
)

MAGIC_V1 = 0xBEEFD00D
MAGIC_V2 = 0xBEEFD00E


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Fuzz Huffman encode/decode")
    p.add_argument("--encode", default="./encode", help="Path to encode binary")
    p.add_argument("--decode", default="./decode", help="Path to decode binary")
    p.add_argument("-n", "--iterations", type=int, default=10_000, help="Number of iterations")
    p.add_argument("--seed", type=int, default=None, help="RNG seed (default: random)")
    p.add_argument("--timeout", type=float, default=1.0, help="Per-process timeout in seconds")
    p.add_argument(
        "--roundtrip-ratio",
        type=float,
        default=0.5,
        help="Fraction of iterations to spend in round-trip mode [0.0, 1.0]",
    )
    p.add_argument(
        "--full-tree-ratio",
        type=float,
        default=0.2,
        help="Fraction of round-trip iterations that use encode -f [0.0, 1.0]",
    )
    p.add_argument(
        "--pipe-ratio",
        type=float,
        default=0.1,
        help="Fraction of round-trip iterations that use stdin/stdout pipelines [0.0, 1.0]",
    )
    p.add_argument(
        "--structured-ratio",
        type=float,
        default=0.1,
        help="Fraction of mutation iterations using structured header mutations [0.0, 1.0]",
    )
    p.add_argument(
        "--max-plain",
        type=int,
        default=256 * 1024,
        help="Max plaintext length for round-trip mode",
    )
    p.add_argument(
        "--max-mutated",
        type=int,
        default=512 * 1024,
        help="Max mutated compressed length",
    )
    p.add_argument(
        "--corpus-limit",
        type=int,
        default=2048,
        help="Maximum number of compressed samples retained for mutation",
    )
    p.add_argument(
        "--out-dir",
        default="fuzz-crashes",
        help="Directory to store failing artifacts",
    )
    p.add_argument(
        "--keep-going",
        action="store_true",
        help="Continue after finding a failure",
    )
    p.add_argument(
        "--progress-every",
        type=int,
        default=100,
        help="Print progress every N iterations",
    )
    return p.parse_args()


def run_command(
    cmd: list[str], timeout: float, input_data: Optional[bytes] = None
) -> tuple[int, bytes, bytes, bool]:
    try:
        proc = subprocess.run(
            cmd,
            input=input_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
        return proc.returncode, proc.stdout, proc.stderr, False
    except subprocess.TimeoutExpired as e:
        stdout = e.stdout if e.stdout is not None else b""
        stderr = e.stderr if e.stderr is not None else b""
        return -999, stdout, stderr, True


def random_bytes(rng: random.Random, max_len: int) -> bytes:
    # Biased length distribution to hit both tiny and medium payloads frequently.
    coin = rng.random()
    if coin < 0.15:
        length = rng.randint(0, 4)
    elif coin < 0.60:
        length = rng.randint(0, 4096)
    else:
        length = rng.randint(0, max_len)
    return rng.randbytes(length)


def mutate_blob(data: bytes, rng: random.Random, max_len: int) -> bytes:
    b = bytearray(data if data else b"\x00")
    for _ in range(rng.randint(1, 8)):
        op = rng.randrange(7)
        if op == 0 and b:
            i = rng.randrange(len(b))
            b[i] ^= 1 << rng.randrange(8)
        elif op == 1 and b:
            i = rng.randrange(len(b))
            b[i] = rng.randrange(256)
        elif op == 2 and b:
            i = rng.randrange(len(b))
            j = rng.randrange(i, len(b))
            del b[i : j + 1]
            if not b:
                b.extend(b"\x00")
        elif op == 3:
            i = rng.randrange(len(b) + 1)
            n = rng.randint(1, 32)
            b[i:i] = rng.randbytes(n)
        elif op == 4 and b:
            i = rng.randrange(len(b))
            j = rng.randrange(i, len(b))
            frag = b[i : j + 1]
            k = rng.randrange(len(b) + 1)
            b[k:k] = frag
        elif op == 5 and len(b) > 1:
            b = b[: rng.randrange(1, len(b) + 1)]
        else:
            n = rng.randint(1, 64)
            b.extend(rng.randbytes(n))
        if len(b) > max_len:
            start = rng.randrange(0, len(b) - max_len + 1)
            b = b[start : start + max_len]
    return bytes(b)


def mutate_structured_header(data: bytes, rng: random.Random, max_len: int) -> bytes:
    # Deterministically perturb semantic header fields.
    # Boundary and sentinel edits on magic/tree_size/file_size/CRC hit parser
    # branches that uniform random bit flips rarely reach.
    b = bytearray(data if data else (MAGIC_V2.to_bytes(4, "little") + b"\x00" * 14))
    if len(b) < 18:
        b.extend(rng.randbytes(18 - len(b)))

    op = rng.randrange(6)
    if op == 0:
        b[0:4] = MAGIC_V1.to_bytes(4, "little")
    elif op == 1:
        b[0:4] = (0xDEADBEEF).to_bytes(4, "little")
    elif op == 2:
        tree_size = rng.choice([5, 767, 4, 768, 0, 0xFFFF])
        b[6:8] = int(tree_size).to_bytes(2, "little", signed=False)
    elif op == 3:
        file_size = rng.choice([0, (1 << 64) - 1])
        b[8:16] = int(file_size).to_bytes(8, "little", signed=False)
    elif op == 4:
        b[16:18] = b"\x00\x00"
    else:
        crc = int.from_bytes(b[16:18], "little", signed=False)
        b[16:18] = (crc ^ 0xFFFF).to_bytes(2, "little", signed=False)

    if len(b) > max_len:
        b = b[:max_len]
    return bytes(b)


def has_sanitizer_diagnostic(stderr: bytes) -> bool:
    text = stderr.decode("utf-8", errors="replace")
    return any(marker in text for marker in SANITIZER_MARKERS)


def safe_read(path: Path) -> bytes:
    try:
        return path.read_bytes() if path.exists() else b""
    except OSError:
        return b""


def save_failure(
    out_dir: Path,
    kind: str,
    iteration: int,
    seed: int,
    payload: Optional[bytes],
    compressed: Optional[bytes],
    decoded: Optional[bytes],
    enc_rc: Optional[int],
    dec_rc: Optional[int],
    enc_stderr: bytes,
    dec_stderr: bytes,
    timed_out: bool,
) -> Path:
    stamp = f"{int(time.time())}_{iteration}_{kind}"
    d = out_dir / stamp
    d.mkdir(parents=True, exist_ok=True)
    meta = [
        f"kind={kind}",
        f"iteration={iteration}",
        f"seed={seed}",
        f"timed_out={timed_out}",
        f"encode_rc={enc_rc}",
        f"decode_rc={dec_rc}",
    ]
    (d / "meta.txt").write_text("\n".join(meta) + "\n", encoding="utf-8")
    if payload is not None:
        (d / "input.bin").write_bytes(payload)
    if compressed is not None:
        (d / "compressed.bin").write_bytes(compressed)
    if decoded is not None:
        (d / "decoded.bin").write_bytes(decoded)
    if enc_stderr:
        (d / "encode.stderr.txt").write_bytes(enc_stderr)
    if dec_stderr:
        (d / "decode.stderr.txt").write_bytes(dec_stderr)
    return d


def make_v1_stream(v2_stream: bytes) -> Optional[bytes]:
    if len(v2_stream) < 18:
        return None
    out = bytearray(v2_stream)
    out[0:4] = MAGIC_V1.to_bytes(4, "little")
    return bytes(out[:16]) + bytes(out[18:])


def seed_valid_corpus(
    corpus: list[bytes], rng: random.Random, encode_bin: str, timeout: float, corpus_limit: int
) -> None:
    # Bootstrap mutation mode with valid compressed samples (including V1 form).
    # This avoids wasting early mutation iterations on junk-only inputs.
    seed_payloads = [
        b"",
        b"A",
        bytes(range(256)),
        b"The quick brown fox jumps over the lazy dog.\n" * 8,
    ]
    for payload in seed_payloads:
        use_full_tree = rng.random() < 0.5
        cmd = [encode_bin]
        if use_full_tree:
            cmd.append("-f")
        enc_rc, compressed, enc_err, enc_to = run_command(cmd, timeout=timeout, input_data=payload)
        if enc_to or enc_rc != 0 or has_sanitizer_diagnostic(enc_err) or not compressed:
            continue
        if len(corpus) < corpus_limit:
            corpus.append(compressed)
        else:
            corpus[rng.randrange(len(corpus))] = compressed
        v1 = make_v1_stream(compressed)
        if v1 is not None:
            if len(corpus) < corpus_limit:
                corpus.append(v1)
            else:
                corpus[rng.randrange(len(corpus))] = v1


def roundtrip_case(
    rng: random.Random,
    work_dir: Path,
    encode_bin: str,
    decode_bin: str,
    timeout: float,
    max_plain: int,
    full_tree_ratio: float,
    pipe_ratio: float,
) -> tuple[bool, str, bytes, bytes, bytes, int, int, bytes, bytes, bool]:
    # End-to-end encode/decode correctness check.
    # Runs in file mode or stdin/stdout pipeline mode and compares payload bytes.
    # Both paths check return codes and sanitizer diagnostics on stderr.
    payload = random_bytes(rng, max_plain)
    use_full_tree = rng.random() < full_tree_ratio
    use_pipe = rng.random() < pipe_ratio

    if use_pipe:
        enc_cmd = [encode_bin]
        if use_full_tree:
            enc_cmd.append("-f")
        enc_rc, compressed, enc_err, enc_to = run_command(enc_cmd, timeout=timeout, input_data=payload)
        if enc_to:
            return False, "encode_timeout", payload, b"", b"", enc_rc, -1, enc_err, b"", True
        if has_sanitizer_diagnostic(enc_err):
            return False, "encode_sanitizer", payload, compressed, b"", enc_rc, -1, enc_err, b"", False
        if enc_rc != 0 or not compressed:
            return False, "encode_fail", payload, compressed, b"", enc_rc, -1, enc_err, b"", False

        dec_rc, decoded, dec_err, dec_to = run_command([decode_bin], timeout=timeout, input_data=compressed)
        if dec_to:
            return False, "decode_timeout", payload, compressed, decoded, enc_rc, dec_rc, enc_err, dec_err, True
        if has_sanitizer_diagnostic(dec_err):
            return (
                False,
                "decode_sanitizer",
                payload,
                compressed,
                decoded,
                enc_rc,
                dec_rc,
                enc_err,
                dec_err,
                False,
            )
        if dec_rc != 0:
            return False, "decode_fail", payload, compressed, decoded, enc_rc, dec_rc, enc_err, dec_err, False
        if decoded != payload:
            return (
                False,
                "roundtrip_mismatch",
                payload,
                compressed,
                decoded,
                enc_rc,
                dec_rc,
                enc_err,
                dec_err,
                False,
            )
        return True, "", payload, compressed, decoded, enc_rc, dec_rc, enc_err, dec_err, False

    in_path = work_dir / "in.bin"
    comp_path = work_dir / "comp.huf"
    out_path = work_dir / "out.bin"
    in_path.write_bytes(payload)
    if comp_path.exists():
        comp_path.unlink()
    if out_path.exists():
        out_path.unlink()

    enc_cmd = [encode_bin]
    if use_full_tree:
        enc_cmd.append("-f")
    enc_cmd.extend(["-i", str(in_path), "-o", str(comp_path)])
    enc_rc, _, enc_err, enc_to = run_command(enc_cmd, timeout=timeout)
    if enc_to:
        return False, "encode_timeout", payload, b"", b"", enc_rc, -1, enc_err, b"", True
    if has_sanitizer_diagnostic(enc_err):
        comp = safe_read(comp_path)
        return False, "encode_sanitizer", payload, comp, b"", enc_rc, -1, enc_err, b"", False
    if enc_rc != 0 or not comp_path.exists():
        comp = safe_read(comp_path)
        return False, "encode_fail", payload, comp, b"", enc_rc, -1, enc_err, b"", False

    compressed = safe_read(comp_path)
    dec_rc, _, dec_err, dec_to = run_command(
        [decode_bin, "-i", str(comp_path), "-o", str(out_path)],
        timeout=timeout,
    )
    if dec_to:
        return False, "decode_timeout", payload, compressed, b"", enc_rc, dec_rc, enc_err, dec_err, True
    if has_sanitizer_diagnostic(dec_err):
        decoded = safe_read(out_path)
        return False, "decode_sanitizer", payload, compressed, decoded, enc_rc, dec_rc, enc_err, dec_err, False
    if dec_rc != 0 or not out_path.exists():
        decoded = safe_read(out_path)
        return False, "decode_fail", payload, compressed, decoded, enc_rc, dec_rc, enc_err, dec_err, False

    decoded = safe_read(out_path)
    if decoded != payload:
        return False, "roundtrip_mismatch", payload, compressed, decoded, enc_rc, dec_rc, enc_err, dec_err, False
    return True, "", payload, compressed, decoded, enc_rc, dec_rc, enc_err, dec_err, False


def decode_mutation_case(
    rng: random.Random,
    work_dir: Path,
    decode_bin: str,
    timeout: float,
    corpus: list[bytes],
    max_mutated: int,
    structured_ratio: float,
) -> tuple[bool, str, bytes, bytes, int, bytes, bool]:
    # Stress decoder robustness with malformed or near-valid compressed blobs.
    # Select a base from corpus, mutate it (random or structured), and run decode.
    # Non-zero exits are allowed; sanitizer output, signals, and timeouts are not.
    base = rng.choice(corpus) if corpus and rng.random() < 0.9 else rng.randbytes(rng.randint(0, 8192))
    if rng.random() < structured_ratio:
        mutated = mutate_structured_header(base, rng, max_mutated)
    else:
        mutated = mutate_blob(base, rng, max_mutated)
    in_path = work_dir / "mut.huf"
    out_path = work_dir / "mut.out"
    in_path.write_bytes(mutated)
    if out_path.exists():
        out_path.unlink()

    dec_rc, _, dec_err, dec_to = run_command(
        [decode_bin, "-i", str(in_path), "-o", str(out_path)],
        timeout=timeout,
    )
    if dec_to:
        return False, "mutation_decode_timeout", mutated, b"", dec_rc, dec_err, True
    if dec_rc < 0:
        return False, "mutation_decode_signal", mutated, b"", dec_rc, dec_err, False
    if has_sanitizer_diagnostic(dec_err):
        decoded = safe_read(out_path)
        return False, "mutation_decode_sanitizer", mutated, decoded, dec_rc, dec_err, False
    return True, "", mutated, b"", dec_rc, dec_err, False


def main() -> int:
    args = parse_args()
    encode_bin = shutil.which(args.encode) if os.path.sep not in args.encode else args.encode
    decode_bin = shutil.which(args.decode) if os.path.sep not in args.decode else args.decode
    if not encode_bin or not Path(encode_bin).exists():
        print(f"error: encode binary not found: {args.encode}", file=sys.stderr)
        return 2
    if not decode_bin or not Path(decode_bin).exists():
        print(f"error: decode binary not found: {args.decode}", file=sys.stderr)
        return 2
    if args.iterations <= 0:
        print("error: --iterations must be > 0", file=sys.stderr)
        return 2
    if not (0.0 <= args.roundtrip_ratio <= 1.0):
        print("error: --roundtrip-ratio must be in [0.0, 1.0]", file=sys.stderr)
        return 2
    if not (0.0 <= args.full_tree_ratio <= 1.0):
        print("error: --full-tree-ratio must be in [0.0, 1.0]", file=sys.stderr)
        return 2
    if not (0.0 <= args.pipe_ratio <= 1.0):
        print("error: --pipe-ratio must be in [0.0, 1.0]", file=sys.stderr)
        return 2
    if not (0.0 <= args.structured_ratio <= 1.0):
        print("error: --structured-ratio must be in [0.0, 1.0]", file=sys.stderr)
        return 2
    if args.timeout <= 0.0:
        print("error: --timeout must be > 0", file=sys.stderr)
        return 2

    seed = args.seed if args.seed is not None else int.from_bytes(os.urandom(8), "little")
    rng = random.Random(seed)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"[fuzz] seed={seed}")
    print(f"[fuzz] encode={Path(encode_bin).resolve()}")
    print(f"[fuzz] decode={Path(decode_bin).resolve()}")

    # Mixed corpus gives broad coverage quickly: start with tiny junk seeds and
    # continuously inject valid compressed artifacts from startup and round-trips.
    corpus: list[bytes] = [b"", b"\x00", b"\xff", b"L", b"I", b"L\x00I"]
    failures = 0
    seed_valid_corpus(corpus, rng, encode_bin, args.timeout, args.corpus_limit)

    with tempfile.TemporaryDirectory(prefix="huff_fuzz_") as tmp:
        work = Path(tmp)
        for i in range(1, args.iterations + 1):
            use_roundtrip = rng.random() < args.roundtrip_ratio
            if use_roundtrip:
                ok, kind, payload, compressed, decoded, enc_rc, dec_rc, enc_err, dec_err, timed_out = roundtrip_case(
                    rng=rng,
                    work_dir=work,
                    encode_bin=encode_bin,
                    decode_bin=decode_bin,
                    timeout=args.timeout,
                    max_plain=args.max_plain,
                    full_tree_ratio=args.full_tree_ratio,
                    pipe_ratio=args.pipe_ratio,
                )
                if ok:
                    if compressed and (len(corpus) < args.corpus_limit or rng.random() < 0.1):
                        if len(corpus) >= args.corpus_limit:
                            corpus[rng.randrange(len(corpus))] = compressed
                        else:
                            corpus.append(compressed)
                else:
                    failures += 1
                    path = save_failure(
                        out_dir=out_dir,
                        kind=kind,
                        iteration=i,
                        seed=seed,
                        payload=payload,
                        compressed=compressed,
                        decoded=decoded,
                        enc_rc=enc_rc,
                        dec_rc=dec_rc,
                        enc_stderr=enc_err,
                        dec_stderr=dec_err,
                        timed_out=timed_out,
                    )
                    print(f"[fuzz][FAIL] iter={i} kind={kind} artifact={path}", file=sys.stderr)
                    if not args.keep_going:
                        return 1
            else:
                ok, kind, mutated, decoded, dec_rc, dec_err, timed_out = decode_mutation_case(
                    rng=rng,
                    work_dir=work,
                    decode_bin=decode_bin,
                    timeout=args.timeout,
                    corpus=corpus,
                    max_mutated=args.max_mutated,
                    structured_ratio=args.structured_ratio,
                )
                if not ok:
                    failures += 1
                    path = save_failure(
                        out_dir=out_dir,
                        kind=kind,
                        iteration=i,
                        seed=seed,
                        payload=None,
                        compressed=mutated,
                        decoded=decoded,
                        enc_rc=None,
                        dec_rc=dec_rc,
                        enc_stderr=b"",
                        dec_stderr=dec_err,
                        timed_out=timed_out,
                    )
                    print(f"[fuzz][FAIL] iter={i} kind={kind} artifact={path}", file=sys.stderr)
                    if not args.keep_going:
                        return 1

            if args.progress_every > 0 and i % args.progress_every == 0:
                print(f"[fuzz] iter={i}/{args.iterations} failures={failures} corpus={len(corpus)}")

    print(f"[fuzz] done iterations={args.iterations} failures={failures}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
