# README #

This README would normally document whatever steps are necessary to get your application up and running.

### What is this repository for? ###

* Optimal static Huffman coding of 8-bit symbols. 
* No attempt is made to make the tree externalization
as small as possible since that saves only a few bytes at the cost of increased complexity.
* Makes extensive use of data structure abstraction (as an example to students).
* Works for both Big and Little Endian architectures.
* Version: 1.0

### How do I get set up? ###

* make

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

* `python3 tests/run_pilot_comparison.py --preset quick --session-limit 90 --out-dir workloads/pilot_runs`
* source data: `workloads/pilot_runs/comparison_summary.csv`

Values are `mean ± CI` in seconds (Pilot quick preset).

| Workload | Operation | C | Rust | Rust speedup |
| --- | --- | --- | --- | --- |
| Shakespeare | encode | `0.0648737 ± 0.001386140` | `0.0464818 ± 0.000721462` | `1.40x` |
| Shakespeare | decode | `0.1007790 ± 0.019890500` | `0.0549753 ± 0.000481145` | `1.83x` |
| Kipling | encode | `0.0175804 ± 0.000282152` | `0.0139301 ± 0.000198056` | `1.26x` |
| Kipling | decode | `0.0256239 ± 0.000867855` | `0.0154264 ± 0.000648532` | `1.66x` |

### Contribution guidelines ###

* Do not try to be overly clever: simplicity and clarity are more
important that exhibiting prowess in obscure C tricks.

### Who do I talk to? ###

* darrell@ucsc.edu
