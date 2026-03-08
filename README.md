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

* `pilot-bench`: <https://github.com/ascar-io/pilot-bench>
* `python3 tests/run_pilot_comparison.py --preset quick --session-limit 600 --out-dir workloads/pilot_runs`
* helper scripts: `tests/pilot_run_local.sh` (default local path), `tests/pilot_run_remote.sh` (generic remote runner; set `REMOTE_HOST`)
* source data: `workloads/pilot_runs/comparison_summary.csv`
* kernel workload source: `workloads/kernel/README.md`

Values are in **seconds**, reported as **mean ± 95% CI**, with **repetitions (`n`)** per case.

| Workload | Operation | C (s, mean ± 95% CI) | C n | Rust (s, mean ± 95% CI) | Rust n | Rust speedup |
| --- | --- | --- | --- | --- | --- | --- |
| Shakespeare | encode | `0.0616302 ± 0.000793960` | `30` | `0.0459261 ± 0.000268996` | `60` | `1.34x` |
| Shakespeare | decode | `0.0744287 ± 0.001117060` | `30` | `0.0692755 ± 0.001561260` | `33` | `1.07x` |
| Kipling | encode | `0.0167349 ± 0.000276520` | `90` | `0.0138308 ± 0.000167485` | `41` | `1.21x` |
| Kipling | decode | `0.0202921 ± 0.000504419` | `42` | `0.0195754 ± 0.000260255` | `30` | `1.04x` |
| Linux kernel 6.19.6 tarball | encode | `1.4250300 ± 0.006152180` | `60` | `0.8113920 ± 0.003594940` | `30` | `1.76x` |
| Linux kernel 6.19.6 tarball | decode | `1.2364500 ± 0.006382270` | `30` | `0.7987680 ± 0.002316010` | `90` | `1.55x` |

### Benchmark citation (BibTeX) ###

Stored in [`REFERENCES.bib`](./REFERENCES.bib):

```bibtex
@inproceedings{li2016pilot,
  author    = {Yan Li},
  title     = {Pilot: A Framework that Understands How to Do Performance Benchmarks The Right Way},
  booktitle = {2016 IEEE 24th International Symposium on Modeling, Analysis and Simulation of Computer and Telecommunication Systems (MASCOTS)},
  year      = {2016},
  month     = sep,
  address   = {London, UK}
}
```

### Contribution guidelines ###

* Do not try to be overly clever: simplicity and clarity are more
important that exhibiting prowess in obscure C tricks.

### Who do I talk to? ###

* darrell@ucsc.edu
