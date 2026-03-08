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

### Fuzzing ###

Build with sanitizers, then run the Python fuzzer:

* `make clean && make CFLAGS='-Wall -Wextra -Wpedantic -Wshadow -Wparentheses -O1 -g -std=c17 -fsanitize=address,undefined' LDFLAGS='-fsanitize=address,undefined'`
* `python3 fuzz_huffman.py --iterations 20000 --timeout 1.0`

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

### Contribution guidelines ###

* Do not try to be overly clever: simplicity and clarity are more
important that exhibiting prowess in obscure C tricks.

### Who do I talk to? ###

* darrell@ucsc.edu
