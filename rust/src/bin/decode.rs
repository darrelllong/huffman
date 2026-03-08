use std::env;
use std::fs::File;
use std::fs::OpenOptions;
use std::io::{self, Read, Write};
use std::path::PathBuf;

use huffman_rs::decode_bytes;

#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;

#[derive(Default)]
struct Args {
    input: Option<PathBuf>,
    output: Option<PathBuf>,
}

fn parse_args() -> Result<Args, String> {
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

fn read_all_input(path: Option<&PathBuf>) -> Result<Vec<u8>, io::Error> {
    let mut data = Vec::new();
    match path {
        Some(p) => File::open(p)?.read_to_end(&mut data)?,
        None => io::stdin().read_to_end(&mut data)?,
    };
    Ok(data)
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
            eprintln!("decode: read input failed: {e}");
            std::process::exit(1);
        }
    };

    let decoded = match decode_bytes(&input) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("decode: {e}");
            std::process::exit(1);
        }
    };
    if !decoded.header_crc_ok && decoded.format_magic != huffman_rs::MAGIC_V1 {
        eprintln!("Warning: header CRC mismatch, using safe fallback permissions 0444.");
    }

    match args.output.as_ref() {
        Some(path) => {
            let mut f = match OpenOptions::new().create_new(true).write(true).open(path) {
                Ok(v) => v,
                Err(e) => {
                    eprintln!("decode: {}: {e}", path.display());
                    std::process::exit(1);
                }
            };

            #[cfg(unix)]
            {
                let mode = decoded.permissions & 0o7777;
                if let Err(e) = f.set_permissions(std::fs::Permissions::from_mode(mode as u32)) {
                    eprintln!("decode: change output file permissions failed: {e}");
                    std::process::exit(1);
                }
            }

            if let Err(e) = f.write_all(&decoded.data) {
                eprintln!("decode: write output failed: {e}");
                std::process::exit(1);
            }
        }
        None => {
            if let Err(e) = io::stdout().write_all(&decoded.data) {
                eprintln!("decode: write stdout failed: {e}");
                std::process::exit(1);
            }
        }
    }
}
