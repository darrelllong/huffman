# Rust Huffman (Bit-Compatible with C)

This directory contains a Rust implementation of the same Huffman
encoder/decoder algorithm used by the C tools at the repo root.

Compatibility targets:

* `encode` writes `MAGIC_V2` (`0xBEEFD00E`)
* header is 16 bytes, followed by a 2-byte CRC16/CCITT-FALSE over header bytes
* tree serialization is post-order with `L <symbol>` leaves and `I` internals
* payload bit order matches C (`LSB`-first within each byte)
* `decode` accepts both `MAGIC_V1` and `MAGIC_V2`
* on `MAGIC_V2` header CRC mismatch, fallback permissions are `0444`

## Build

```bash
cd rust
cargo build --release
```

## Run

```bash
cargo run --release --bin encode -- -i input.bin -o output.huf
cargo run --release --bin decode -- -i output.huf -o recovered.bin
```

## Test

```bash
cd rust
cargo test
```

## Pilot Benchmark Inputs

Use the Gutenberg corpus in `../workloads/gutenberg/`:

* `../workloads/gutenberg/shakespeare_complete_works_100-0.txt`
* `../workloads/gutenberg/kipling_collected_workload.txt`

Build both implementations first:

```bash
cd ..
make
cd rust
cargo build --release
```

Then benchmark C (`../encode`, `../decode`) vs Rust (`target/release/encode`,
`target/release/decode`) with Pilot using the same input files.
