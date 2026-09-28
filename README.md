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

* `pilot-bench`: <https://github.com/darrelllong/pilot-bench>, commit `f01eec4`
* `python3 tests/run_pilot_comparison.py --preset quick --session-limit 600 --cpu 9 --out-dir workloads/pilot_runs`
* helper scripts: `tests/pilot_run_local.sh` (default local path; set `CPU` to pin), `tests/pilot_run_remote.sh` (generic remote runner; set `REMOTE_HOST`)
* source data: `workloads/pilot_runs/comparison_summary.csv`
* kernel workload source: `workloads/kernel/README.md`

Machine and method: knuth, NVIDIA GB10, Ubuntu 24.04.5 LTS (Linux
7.0.0-1019-nvidia, aarch64), `performance` governor. Pilot, the Python
case runner and the program under test were pinned together to logical
CPU 9, a Cortex-X925 core at up to 3.9 GHz. C built with GCC 13.3.0 and
the `Makefile` flags (`-O3 -DNDEBUG -std=c17`); Rust 1.95.0,
`cargo build --release`; Python 3.12.3. Run on September 28, 2026,
20:31–20:39 UTC, with no other benchmark or build running.

Each reading is the wall-clock time, measured by `tests/run_huffman_case.py`
with `time.perf_counter()`, of one invocation of `encode` or `decode`: process
start, reading the input file, coding, and writing the output to a
temporary file. Pilot's `quick` preset requires at least 30 subsession
samples, a 95% confidence interval no wider than 20% of the mean, and
autocorrelation within $\pm 0.8$. Every session converged (Pilot exit status 0).

Values are in **seconds**, reported as **mean $\pm$ half-width of the 95% CI**,
with **repetitions (`n`)**, the number of readings Pilot took. Rust speedup
is C mean divided by Rust mean.

| Workload | Operation | C (s, mean $\pm$ 95% CI) | C n | Rust (s, mean $\pm$ 95% CI) | Rust n | Rust speedup |
| --- | --- | --- | --- | --- | --- | --- |
| Shakespeare | encode | $0.0541423 \pm 0.0000326$ | `53` | $0.0418088 \pm 0.0000588$ | `64` | `1.29x` |
| Shakespeare | decode | $0.0800062 \pm 0.0000364$ | `33` | $0.0652912 \pm 0.0005237$ | `41` | `1.23x` |
| Kipling | encode | $0.0134534 \pm 0.0000124$ | `30` | $0.0105970 \pm 0.0000139$ | `30` | `1.27x` |
| Kipling | decode | $0.0193806 \pm 0.0002270$ | `84` | $0.0164183 \pm 0.0000130$ | `30` | `1.18x` |
| Linux kernel 6.19.6 tarball | encode | $1.7691900 \pm 0.0008656$ | `90` | $1.0768500 \pm 0.0005634$ | `30` | `1.64x` |
| Linux kernel 6.19.6 tarball | decode | $2.1125400 \pm 0.0017334$ | `30` | $1.1954300 \pm 0.0003694$ | `124` | `1.77x` |

The previous snapshot (March 2026, machine not recorded, Pilot before
`f01eec4`) reported the full width of the confidence interval after the $\pm$;
the summary CSV now records both the full width and the half-width. Its
speedups were 1.34x, 1.07x, 1.21x, 1.04x, 1.76x and 1.55x in the order of
the rows above: Rust was faster in every case then as now, and the decode
speedups are larger on knuth. Because the machine, the compiler and Pilot
all changed, the comparison does not attribute the difference to any one
of them.

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
