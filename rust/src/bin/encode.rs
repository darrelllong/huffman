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
    if let Some(p) = path {
        if let Ok(meta) = p.metadata() {
            return meta.permissions().mode() as u16;
        }
    }
    0o644
}

#[cfg(not(unix))]
fn input_mode(_path: Option<&PathBuf>) -> u16 {
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
    let output = match encode_bytes(&input, mode, args.full_tree) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("encode: {e}");
            std::process::exit(1);
        }
    };

    match args.output.as_ref() {
        Some(path) => {
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
            if let Err(e) = io::stdout().write_all(&output) {
                eprintln!("encode: write stdout failed: {e}");
                std::process::exit(1);
            }
        }
    }
}
