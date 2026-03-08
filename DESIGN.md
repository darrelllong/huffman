# Design Notes

This document describes the design and implementation of the Huffman
encoder/decoder in this repository, with emphasis on the C reference
implementation and Rust compatibility.

## Scope and Goals

The project implements optimal static Huffman coding over 8-bit symbols.

Design goals:

1. Bit-for-bit deterministic output for equivalent inputs.
2. Simple, auditable data structures over clever machinery.
3. Portable on little-endian and big-endian hosts.
4. Robust I/O handling for files and pipes.
5. C and Rust implementations that interoperate at the file-format level.

This is not an adaptive Huffman implementation and does not optimize for the
smallest possible tree serialization.

## File Format

Compressed files are self-describing:

1. 16-byte header.
2. 2-byte CRC16 over that header (V2 only).
3. Serialized Huffman tree.
4. Bit-packed payload.

### Header Layout

| Offset | Size | Field | Meaning |
|---|---:|---|---|
| 0 | 4 | `magic` | `0xBEEFD00E` (V2) or `0xBEEFD00D` (V1 legacy) |
| 4 | 2 | `permissions` | Unix mode bits from source file |
| 6 | 2 | `tree_size` | Serialized tree size in bytes |
| 8 | 8 | `file_size` | Original plaintext size in bytes |
| 16 | 2 | `header_crc` | CRC-16/CCITT-FALSE over bytes `[0..16)` (V2 only) |

All multi-byte fields are encoded little-endian.

### Tree Serialization

Post-order encoding:

1. Leaf: byte `'L'`, then one symbol byte.
2. Interior: serialize left subtree, then right subtree, then byte `'I'`.

If a tree has `k` leaves, serialized size is `3k - 1`.
In symbols: $S_{\text{tree}} = 3k - 1$.

Decoder validation enforces:

1. $S_{\text{tree}} \ge 5$
2. $S_{\text{tree}} \le 767$
3. $S_{\text{tree}} \equiv 2 \pmod{3}$

## Core Data Structures (C)

### `treeNode` (`struct DAH`)

Represents both leaf and interior nodes:

1. `symbol`: meaningful for leaves.
2. `count`: frequency on leaves, subtree sum on interiors.
3. `leaf`: node-kind discriminator.
4. `left/right`: child pointers.

`delTree` recursively frees nodes in post-order.

### Priority Queue (`queue`, `priority.c`)

A circular buffer of `treeNode *`, maintained sorted by frequency.

1. `dequeue` removes smallest count in O(1) from `tail`.
2. `enqueue` insertion-sorts in O(n) by shifting larger entries.

Given at most 256 symbols, this simple approach is fast and easy to audit.
Tie behavior is stable for equal counts, which matters for deterministic output.

### Stack (`stack.c`)

Used during tree deserialization:

1. Backing array starts at 256 entries.
2. `push` doubles capacity on demand with safe `realloc` handling.
3. `pop` returns `NULL` on empty stack.

## Encoding Pipeline (C)

`encode.c` performs:

1. Read input and build histogram (`uint64_t hist[256]`).
2. Ensure at least two symbols by adding phantom stand-ins when needed.
3. Build Huffman tree with greedy merge loop.
4. Build per-symbol bit codes with DFS (`buildCode`).
5. Emit header + CRC + serialized tree.
6. Re-read plaintext and emit encoded payload bits.

### Bit Packing

Payload bits are written least-significant-bit first within each output byte.
`appendCode` and `flushCode` in `code.h` maintain a 1 KB bit buffer and write
through `io_write_full`.

## Decoding Pipeline (C)

`decode.c` performs:

1. Exact-read header (`io_read_full`).
2. Validate magic, parse metadata, and read CRC for V2.
3. On CRC mismatch, continue decode with fallback permissions `0444`.
4. Exact-read serialized tree and reconstruct with stack.
5. Decode payload bitstream by tree walk until `file_size` symbols are emitted.

Decode stops by symbol count, not by EOF, so zero-padding bits at the end of
the payload are ignored safely.

## I/O Reliability Model

`src/io.h` centralizes retry loops:

1. `io_read_full`: keep reading until `len` bytes or failure/EOF.
2. `io_write_full`: keep writing until all bytes are written or failure.

These helpers are used for metadata reads and all high-value write paths. This
avoids silent short-read/short-write truncation on pipes or congested I/O.

## Endianness and CRC

### Endianness

Runtime helpers in `endian.h` provide:

1. Host endianness detection (`isBig()`).
2. Byte-swap helpers (`swap16/32/64`).

Fields are always stored little-endian on disk.

### CRC

CRC variant: CRC-16/CCITT-FALSE

1. Polynomial: `0x1021`
2. Init: `0xFFFF`
3. No reflection
4. No XOR-out

CRC protects header integrity only, not the tree or payload.

## C and Rust Compatibility

Rust (`rust/`) is kept wire-compatible with C:

1. Same header layout and CRC policy.
2. Same tree serialization.
3. Same LSB-first bit ordering.
4. Same V1/V2 decode compatibility behavior.
5. Same fallback permissions behavior on V2 CRC mismatch.

Rust decode path is streaming in the CLI, mirroring C behavior for large files.

## Complexity

Let $n$ be input length and $k \le 256$ the number of distinct symbols.

1. Histogram: $O(n)$
2. Tree build: $O(k^2)$ worst-case due to insertion-sort enqueue
3. Code generation: $O(k)$
4. Encode payload walk: $O\!\left(n \times \bar{\ell}\right)$
5. Decode payload walk: $O\!\left(n \times \bar{\ell}\right)$

Here $\bar{\ell}$ is the average code length in bits. Because $k$ is bounded by
$256$, queue complexity is effectively constant-scale in practice.

## Security and Failure Semantics

The implementation is designed for robustness, not adversarial hardening:

1. Reject malformed magic/tree encodings.
2. Refuse to clobber existing output files (`O_CREAT|O_EXCL` / `create_new`).
3. Preserve deterministic decode behavior under header CRC mismatch.
4. Abort on allocation failures in critical paths.

Out of scope:

1. Side-channel resistance.
2. Cryptographic integrity/authentication of payload bytes.
3. Constant-memory decode under all malformed inputs.

## Testing and Fuzzing Strategy

Coverage comes from:

1. C build with strict warnings and sanitizer mode.
2. Rust unit and compatibility tests.
3. Cross-language bit-compatibility checks.
4. Mutation + round-trip fuzzer with:
   - random and structured header mutations,
   - stdin/stdout and file-path execution modes,
   - V1 and V2 corpus entries,
   - sanitizer-diagnostic detection in encoder and decoder stderr.

## Module Map

| File | Responsibility |
|---|---|
| `src/encode.c` | C encoder entry point |
| `src/decode.c` | C decoder entry point |
| `src/huffman.c` / `src/huffman.h` | Tree node construction and tree utilities |
| `src/priority.c` / `src/queue.h` | Priority queue implementation/interface |
| `src/stack.c` / `src/stack.h` | Dynamic stack for deserialization |
| `src/code.h` | Code representation and payload bit-buffer logic |
| `src/header.h` | On-disk header definition and magic values |
| `src/endian.h` | Endianness detection and byte swaps |
| `src/crc16.h` | CRC-16/CCITT-FALSE implementation |
| `src/io.h` | Exact read/write helpers |
| `tests/fuzz_huffman.py` | Coverage-oriented black-box fuzzer |
| `rust/src/lib.rs` | Rust format-compatible encoder/decoder core |
| `rust/src/bin/encode.rs` | Rust CLI encoder |
| `rust/src/bin/decode.rs` | Rust CLI decoder |
