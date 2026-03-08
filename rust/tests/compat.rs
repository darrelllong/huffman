use std::path::{Path, PathBuf};
use std::process::Command;

use huffman_rs::{FALLBACK_PERMISSIONS, MAGIC_V1, decode_bytes, encode_bytes};

fn repo_root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("rust/ has a parent")
        .to_path_buf()
}

fn ensure_c_binaries() -> Option<(PathBuf, PathBuf)> {
    let root = repo_root();
    let enc = root.join("encode");
    let dec = root.join("decode");
    if enc.exists() && dec.exists() {
        return Some((enc, dec));
    }

    let status = Command::new("make").current_dir(&root).status().ok()?;
    if !status.success() {
        return None;
    }
    if enc.exists() && dec.exists() {
        Some((enc, dec))
    } else {
        None
    }
}

fn test_payload() -> Vec<u8> {
    let mut data = Vec::new();
    for i in 0..(256u32 * 32) {
        data.push((i as u8).wrapping_mul(17).wrapping_add(23));
    }
    data.extend_from_slice(b"The quick brown fox jumps over the lazy dog.\n");
    data
}

#[test]
fn rust_encode_is_byte_compatible_with_c_encode_v2() {
    let Some((c_encode, _)) = ensure_c_binaries() else {
        eprintln!("skipping: C encode/decode binaries unavailable");
        return;
    };

    let tmp = tempfile::tempdir().expect("tempdir");
    let input_path = tmp.path().join("input.bin");
    let c_out_path = tmp.path().join("c.huf");
    let payload = test_payload();
    std::fs::write(&input_path, &payload).expect("write input");

    let status = Command::new(c_encode)
        .arg("-i")
        .arg(&input_path)
        .arg("-o")
        .arg(&c_out_path)
        .status()
        .expect("run C encode");
    assert!(status.success(), "C encode failed");

    #[cfg(unix)]
    let mode = {
        use std::os::unix::fs::PermissionsExt;
        input_path
            .metadata()
            .expect("metadata")
            .permissions()
            .mode() as u16
    };
    #[cfg(not(unix))]
    let mode = 0o644u16;

    let rust_bytes = encode_bytes(&payload, mode, false).expect("rust encode");
    let c_bytes = std::fs::read(c_out_path).expect("read c output");
    assert_eq!(rust_bytes, c_bytes, "Rust and C encoded outputs differ");
}

#[test]
fn rust_decode_accepts_v2_and_v1_layouts() {
    let payload = test_payload();
    let mut encoded = encode_bytes(&payload, 0o644, false).expect("encode");

    let decoded_v2 = decode_bytes(&encoded).expect("decode v2");
    assert_eq!(decoded_v2.data, payload);
    assert!(decoded_v2.header_crc_ok);

    // Build a synthetic v1 stream by replacing magic and dropping CRC bytes.
    encoded[0..4].copy_from_slice(&MAGIC_V1.to_le_bytes());
    let mut v1 = Vec::with_capacity(encoded.len() - 2);
    v1.extend_from_slice(&encoded[..16]);
    v1.extend_from_slice(&encoded[18..]);
    let decoded_v1 = decode_bytes(&v1).expect("decode v1");
    assert_eq!(decoded_v1.data, payload);
}

#[test]
fn rust_decode_uses_fallback_permissions_when_crc_is_bad() {
    let payload = test_payload();
    let mut encoded = encode_bytes(&payload, 0o700, false).expect("encode");
    // Flip one permission bit in header but keep parseable sizes.
    encoded[4] ^= 0x01;
    let decoded = decode_bytes(&encoded).expect("decode");
    assert_eq!(decoded.data, payload);
    assert!(!decoded.header_crc_ok);
    assert_eq!(decoded.permissions, FALLBACK_PERMISSIONS);
}
