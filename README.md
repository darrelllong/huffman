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

* `python3 tests/run_pilot_comparison.py --preset quick --session-limit 600 --out-dir workloads/pilot_runs`
* source data: `workloads/pilot_runs/comparison_summary.csv`
* kernel workload source: `workloads/kernel/README.md`

Values are in **seconds**, with **95% CI width** and **repetitions (`n`)** reported per case.

| Workload | Operation | C mean (s) | C CI95 width (s) | C n | Rust mean (s) | Rust CI95 width (s) | Rust n | Rust speedup |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Shakespeare | encode | `0.0638362` | `0.000646338` | `30` | `0.0459180` | `0.000308300` | `33` | `1.39x` |
| Shakespeare | decode | `0.0950250` | `0.000648952` | `104` | `0.0553165` | `0.000972752` | `30` | `1.72x` |
| Kipling | encode | `0.0174725` | `0.000271299` | `90` | `0.0140829` | `0.000905666` | `123` | `1.24x` |
| Kipling | decode | `0.0257757` | `0.001915760` | `30` | `0.0151876` | `0.000286223` | `36` | `1.70x` |
| Linux kernel 6.19.6 tarball | encode | `1.6701200` | `0.005375330` | `121` | `0.8236210` | `0.003724750` | `30` | `2.03x` |
| Linux kernel 6.19.6 tarball | decode | `2.5661500` | `0.012740400` | `30` | `3.1048300` | `0.016188800` | `30` | `0.83x` |

### Contribution guidelines ###

* Do not try to be overly clever: simplicity and clarity are more
important that exhibiting prowess in obscure C tricks.

### Who do I talk to? ###

* darrell@ucsc.edu
