//! CLI front-end for Huffman decoding.
//!
//! This wrapper is intentionally thin: it handles argument parsing and I/O
//! policy, then delegates format logic to `huffman_rs::decode_stream`.

use std::env;
use std::fs::File;
use std::fs::OpenOptions;
use std::io::{self, Read, Write};
use std::path::PathBuf;

use huffman_rs::decode_stream;

#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;

#[derive(Default)]
struct Args {
    input: Option<PathBuf>,
    output: Option<PathBuf>,
}

fn parse_args() -> Result<Args, String> {
    // Parse decode CLI flags into typed inputs/outputs.
    // Missing values and unknown flags fail immediately.
    let mut args = Args::default();
    let mut it = env::args().skip(1);
    while let Some(arg) = it.next() {
        match arg.as_str() {
            "-i" | "--input" => {
                let Some(v) = it.next() else {
                    return Err("missing value for --input".into());
                };
                args.input = Some(PathBuf::from(v));
            }
            "-o" | "--output" => {
                let Some(v) = it.next() else {
                    return Err("missing value for --output".into());
                };
                args.output = Some(PathBuf::from(v));
            }
            "-h" | "--help" => {
                return Err("usage: decode [-i input] [-o output]".into());
            }
            _ => return Err(format!("unknown argument: {arg}")),
        }
    }
    Ok(args)
}

fn main() {
    let args = match parse_args() {
        Ok(v) => v,
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    };

    match args.output.as_ref() {
        Some(path) => {
            // Create output atomically and fail if the file already exists.
            // This keeps behavior aligned with the C decoder.
            let mut f = match OpenOptions::new().create_new(true).write(true).open(path) {
                Ok(v) => v,
                Err(e) => {
                    eprintln!("decode: {}: {e}", path.display());
                    std::process::exit(1);
                }
            };

            // Support both file input and stdin piping in one control path
            // by selecting a boxed Read implementation at runtime.
            let mut input: Box<dyn Read> = match args.input.as_ref() {
                Some(p) => match File::open(p) {
                    Ok(v) => Box::new(v),
                    Err(e) => {
                        eprintln!("decode: read input failed: {e}");
                        std::process::exit(1);
                    }
                },
                None => Box::new(io::stdin().lock()),
            };

            // Read header/tree and decode directly into the output file.
            // decode_stream avoids whole-file buffering by streaming output
            // incrementally as each symbol is decoded.
            let decoded = match decode_stream(&mut input, &mut f) {
                Ok(v) => v,
                Err(e) => {
                    eprintln!("decode: {e}");
                    std::process::exit(1);
                }
            };

            if !decoded.header_crc_ok && decoded.format_magic != huffman_rs::MAGIC_V1 {
                // Preserve compatibility with legacy V1 streams:
                // only warn on CRC mismatch for V2, where CRC exists.
                eprintln!("Warning: header CRC mismatch, using safe fallback permissions 0444.");
            }

            #[cfg(unix)]
            {
                // Restore decoded mode from trusted/fallback header metadata.
                // Apply mode after successful decode so partial outputs are not chmod'd first.
                let mode = decoded.permissions & 0o7777;
                if let Err(e) = f.set_permissions(std::fs::Permissions::from_mode(mode as u32)) {
                    eprintln!("decode: change output file permissions failed: {e}");
                    std::process::exit(1);
                }
            }
        }
        None => {
            // Pipeline mode: stdin -> decode -> stdout using locked streams
            // to avoid per-byte locking overhead.
            let mut stdin = io::stdin().lock();
            let mut stdout = io::stdout().lock();
            let decoded = match decode_stream(&mut stdin, &mut stdout) {
                Ok(v) => v,
                Err(e) => {
                    eprintln!("decode: {e}");
                    std::process::exit(1);
                }
            };
            if !decoded.header_crc_ok && decoded.format_magic != huffman_rs::MAGIC_V1 {
                eprintln!("Warning: header CRC mismatch, using safe fallback permissions 0444.");
            }
            if let Err(e) = stdout.flush() {
                eprintln!("decode: write stdout failed: {e}");
                std::process::exit(1);
            }
        }
    }
}
