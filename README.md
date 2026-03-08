# Huffman #

Reference implementation of optimal static Huffman coding for 8-bit symbols,
with matching C and Rust encoders/decoders.

### What is this repository for? ###

* Optimal static Huffman coding of 8-bit symbols. 
* No attempt is made to make the tree externalization
as small as possible since that saves only a few bytes at the cost of increased complexity.
* Makes extensive use of data structure abstraction (as an example to students).
* Works for both Big and Little Endian architectures.
* Version: 1.0

### How do I get set up? ###

Build C tools:

* `make`

Build Rust tools:

* `cd rust && cargo build --release`

Run Rust tests:

* `cd rust && cargo test`

### Layout ###

* `src/` contains the C implementation (`encode`, `decode`, and shared modules).
* `tests/` contains fuzzing and benchmark helper scripts.
* `rust/` contains the bit-compatible Rust implementation.

### Fuzzing ###

Build with sanitizers, then run the Python fuzzer:

* `make clean && make CFLAGS='-Wall -Wextra -Wpedantic -Wshadow -Wparentheses -O1 -g -std=c17 -fsanitize=address,undefined' LDFLAGS='-fsanitize=address,undefined'`
* `python3 tests/fuzz_huffman.py --iterations 20000 --timeout 1.0`

The fuzzer exercises both valid round-trip cases (`encode` -> `decode`) and
mutated malformed streams for `decode`. Any crash, timeout, sanitizer report,
or round-trip mismatch is saved under `fuzz-crashes/`.

### Bitstream compatibility ###

Current output format is versioned and intended to be reproduced bit-for-bit:

* Header magic `MAGIC_V2` (`0xBEEFD00E`)
* 16-byte header
* 2-byte CRC16/CCITT-FALSE over exactly those 16 header bytes
* Serialized Huffman tree bytes
* Encoded payload bitstream

`decode` accepts both `MAGIC_V1` and `MAGIC_V2`. A Rust implementation should
emit `MAGIC_V2` with the same CRC16 placement and preserve this byte layout.

### Pilot timing snapshot ###

The following timing snapshot comes from:

* `python3 tests/run_pilot_comparison.py --preset quick --session-limit 600 --out-dir workloads/pilot_runs`
* source data: `workloads/pilot_runs/comparison_summary.csv`
* kernel workload source: `workloads/kernel/README.md`

Values are in **seconds**, reported as **mean ± 95% CI**, with **repetitions (`n`)** per case.

| Workload | Operation | C (s, mean ± 95% CI) | C n | Rust (s, mean ± 95% CI) | Rust n | Rust speedup |
| --- | --- | --- | --- | --- | --- | --- |
| Shakespeare | encode | `0.0619383 ± 0.000635901` | `60` | `0.0460462 ± 0.000652528` | `112` | `1.35x` |
| Shakespeare | decode | `0.0785577 ± 0.015631300` | `50` | `0.0545279 ± 0.000555148` | `63` | `1.44x` |
| Kipling | encode | `0.0169466 ± 0.000654773` | `57` | `0.0138650 ± 0.000172161` | `35` | `1.22x` |
| Kipling | decode | `0.0204930 ± 0.000471669` | `30` | `0.0151804 ± 0.000263268` | `90` | `1.35x` |
| Linux kernel 6.19.6 tarball | encode | `1.4267400 ± 0.009018640` | `30` | `0.8178160 ± 0.002684840` | `48` | `1.74x` |
| Linux kernel 6.19.6 tarball | decode | `1.2403200 ± 0.009235380` | `60` | `3.0972300 ± 0.008790740` | `30` | `0.40x` |

### Contribution guidelines ###

* Do not try to be overly clever: simplicity and clarity are more
important that exhibiting prowess in obscure C tricks.

### Who do I talk to? ###

* darrell@ucsc.edu
