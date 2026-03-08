//! CLI front-end for Huffman encoding.
//!
//! This wrapper is intentionally thin: it handles argument parsing and I/O
//! policy, then delegates format logic to `huffman_rs::encode_bytes`.

use std::env;
use std::fs::File;
use std::fs::OpenOptions;
use std::io::{self, Read, Write};
use std::path::PathBuf;

use huffman_rs::encode_bytes;

#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;

#[derive(Default)]
struct Args {
    input: Option<PathBuf>,
    output: Option<PathBuf>,
    full_tree: bool,
}

fn parse_args() -> Result<Args, String> {
    // Keep argument parsing strict so caller mistakes fail immediately.
    // We treat help as a usage error path to keep one simple exit flow.
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
            "-f" | "--full" => args.full_tree = true,
            "-h" | "--help" => {
                return Err("usage: encode [-f] [-i input] [-o output]".into());
            }
            _ => return Err(format!("unknown argument: {arg}")),
        }
    }
    Ok(args)
}

fn read_all_input(path: Option<&PathBuf>) -> Result<Vec<u8>, io::Error> {
    // Encoder builds a histogram before emitting payload bits, so we need one
    // complete input buffer here (matching the C encoder's full-input model).
    let mut data = Vec::new();
    match path {
        Some(p) => {
            File::open(p)?.read_to_end(&mut data)?;
        }
        None => {
            io::stdin().read_to_end(&mut data)?;
        }
    }
    Ok(data)
}

#[cfg(unix)]
fn input_mode(path: Option<&PathBuf>) -> u16 {
    // Preserve source permission bits in the compressed header when possible.
    // Fall back to 0644 if metadata cannot be read.
    if let Some(p) = path {
        if let Ok(meta) = p.metadata() {
            return meta.permissions().mode() as u16;
        }
    }
    0o644
}

#[cfg(not(unix))]
fn input_mode(_path: Option<&PathBuf>) -> u16 {
    // Non-Unix targets do not carry Unix mode bits in portable metadata.
    0o644
}

fn main() {
    let args = match parse_args() {
        Ok(v) => v,
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    };

    let input = match read_all_input(args.input.as_ref()) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("encode: read input failed: {e}");
            std::process::exit(1);
        }
    };

    let mode = input_mode(args.input.as_ref());
    // Encode with the same header/tree/payload layout as the C implementation.
    // The optional full-tree mode forces all 256 symbols into the tree.
    let output = match encode_bytes(&input, mode, args.full_tree) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("encode: {e}");
            std::process::exit(1);
        }
    };

    match args.output.as_ref() {
        Some(path) => {
            // Avoid clobbering existing files and eliminate TOCTOU races:
            // one atomic open call either creates the file or fails.
            let mut f = match OpenOptions::new().create_new(true).write(true).open(path) {
                Ok(v) => v,
                Err(e) => {
                    eprintln!("encode: {}: {e}", path.display());
                    std::process::exit(1);
                }
            };
            if let Err(e) = f.write_all(&output) {
                eprintln!("encode: write output failed: {e}");
                std::process::exit(1);
            }
        }
        None => {
            // Pipeline mode: emit compressed bytes to stdout when -o is omitted.
            if let Err(e) = io::stdout().write_all(&output) {
                eprintln!("encode: write stdout failed: {e}");
                std::process::exit(1);
            }
        }
    }
}
