use std::collections::VecDeque;
use std::fmt;
use std::io;

pub const MAGIC_V1: u32 = 0xBEEFD00D;
pub const MAGIC_V2: u32 = 0xBEEFD00E;
pub const MAGIC: u32 = MAGIC_V2;
pub const FALLBACK_PERMISSIONS: u16 = 0o444;
const BYTE_VALUES: usize = 256;
const CODE_BITS: usize = 256;
const CODE_BYTES: usize = CODE_BITS / 8;
const MIN_TREE_BYTES: usize = 5;
const MAX_TREE_BYTES: usize = 3 * BYTE_VALUES - 1;

#[derive(Debug)]
pub enum HuffmanError {
    Io(io::Error),
    InvalidFormat(&'static str),
}

impl fmt::Display for HuffmanError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(e) => write!(f, "{e}"),
            Self::InvalidFormat(s) => write!(f, "{s}"),
        }
    }
}

impl std::error::Error for HuffmanError {}

impl From<io::Error> for HuffmanError {
    fn from(value: io::Error) -> Self {
        Self::Io(value)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Header {
    pub magic: u32,
    pub permissions: u16,
    pub tree_size: u16,
    pub file_size: u64,
}

#[derive(Debug)]
pub struct DecodeResult {
    pub data: Vec<u8>,
    pub permissions: u16,
    pub header_crc_ok: bool,
    pub format_magic: u32,
}

#[derive(Debug)]
struct Node {
    symbol: u8,
    count: u64,
    leaf: bool,
    left: Option<Box<Node>>,
    right: Option<Box<Node>>,
}

impl Node {
    fn new_leaf(symbol: u8, count: u64) -> Self {
        Self {
            symbol,
            count,
            leaf: true,
            left: None,
            right: None,
        }
    }

    fn join(left: Self, right: Self) -> Self {
        Self {
            symbol: b'$',
            count: left.count + right.count,
            leaf: false,
            left: Some(Box::new(left)),
            right: Some(Box::new(right)),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Code {
    bits: [u8; CODE_BYTES],
    len: u16,
}

impl Code {
    const EMPTY: Self = Self {
        bits: [0; CODE_BYTES],
        len: 0,
    };

    fn set_bit(&mut self, i: usize, bit: u8) {
        if bit != 0 {
            self.bits[i / 8] |= 1u8 << (i % 8);
        }
    }

    fn bit(&self, i: usize) -> u8 {
        (self.bits[i / 8] >> (i % 8)) & 0x1
    }
}

// Queue discipline follows the C code: insertion sort by frequency.
// Ties preserve arrival order (stable for equal counts).
fn enqueue_sorted(queue: &mut VecDeque<Node>, node: Node) {
    let slot = queue
        .iter()
        .position(|entry| entry.count > node.count)
        .unwrap_or(queue.len());
    queue.insert(slot, node);
}

fn build_tree(input: &[u8], full_tree: bool) -> Result<(Node, u16), HuffmanError> {
    let mut hist = [0u64; BYTE_VALUES];
    let mut unique = 0u16;

    for &b in input {
        let idx = b as usize;
        if hist[idx] == 0 {
            unique += 1;
        }
        hist[idx] += 1;
    }

    // Require at least two symbols, exactly like the C implementation.
    if unique < 2 {
        if hist[0] == 0 {
            hist[0] += 1;
        }
        if hist[0xFF] == 0 {
            hist[0xFF] += 1;
        }
    }

    let mut queue: VecDeque<Node> = VecDeque::with_capacity(BYTE_VALUES + 1);
    let mut leaves: u16 = 0;
    for (i, &count) in hist.iter().enumerate() {
        if full_tree || count > 0 {
            enqueue_sorted(&mut queue, Node::new_leaf(i as u8, count));
            leaves += 1;
        }
    }

    let tree_size = if leaves > 0 { 3 * leaves - 1 } else { 0 };
    let mut root: Option<Node> = None;
    while let Some(left) = queue.pop_front() {
        if queue.is_empty() {
            root = Some(left);
        } else {
            let right = queue
                .pop_front()
                .expect("queue is non-empty because one element was checked");
            enqueue_sorted(&mut queue, Node::join(left, right));
        }
    }

    let Some(root) = root else {
        return Err(HuffmanError::InvalidFormat("failed to build tree"));
    };
    Ok((root, tree_size))
}

fn dump_tree(node: &Node, out: &mut Vec<u8>) {
    // Post-order serialization:
    //   leaf => 'L' + symbol
    //   internal => left, right, 'I'
    if node.leaf {
        out.push(b'L');
        out.push(node.symbol);
        return;
    }
    let left = node.left.as_ref().expect("internal nodes have left child");
    let right = node
        .right
        .as_ref()
        .expect("internal nodes have right child");
    dump_tree(left, out);
    dump_tree(right, out);
    out.push(b'I');
}

fn build_codes(root: &Node) -> [Code; BYTE_VALUES] {
    fn rec(
        node: &Node,
        path: &mut [u8; BYTE_VALUES],
        depth: usize,
        codes: &mut [Code; BYTE_VALUES],
    ) {
        if node.leaf {
            let mut code = Code::EMPTY;
            code.len = depth as u16;
            for (i, bit) in path[..depth].iter().copied().enumerate() {
                code.set_bit(i, bit);
            }
            codes[node.symbol as usize] = code;
            return;
        }

        let left = node.left.as_ref().expect("internal nodes have left child");
        path[depth] = 0;
        rec(left, path, depth + 1, codes);

        let right = node
            .right
            .as_ref()
            .expect("internal nodes have right child");
        path[depth] = 1;
        rec(right, path, depth + 1, codes);
    }

    let mut codes = [Code::EMPTY; BYTE_VALUES];
    let mut path = [0u8; BYTE_VALUES];
    rec(root, &mut path, 0, &mut codes);
    codes
}

fn encode_payload(input: &[u8], codes: &[Code; BYTE_VALUES]) -> Vec<u8> {
    // Output bit order matches C: least-significant bit first in each byte.
    let total_bits: usize = input.iter().map(|&b| codes[b as usize].len as usize).sum();
    let mut out = Vec::with_capacity(total_bits.div_ceil(8));
    let mut current = 0u8;
    let mut bit_pos = 0usize;
    for &b in input {
        let code = codes[b as usize];
        for i in 0..code.len as usize {
            if code.bit(i) != 0 {
                current |= 1 << bit_pos;
            }
            bit_pos += 1;
            if bit_pos == 8 {
                out.push(current);
                current = 0;
                bit_pos = 0;
            }
        }
    }
    if bit_pos != 0 {
        out.push(current);
    }
    out
}

fn write_header_bytes(header: Header) -> [u8; 16] {
    let mut out = [0u8; 16];
    out[0..4].copy_from_slice(&header.magic.to_le_bytes());
    out[4..6].copy_from_slice(&header.permissions.to_le_bytes());
    out[6..8].copy_from_slice(&header.tree_size.to_le_bytes());
    out[8..16].copy_from_slice(&header.file_size.to_le_bytes());
    out
}

fn read_header_bytes(src: &[u8]) -> Result<Header, HuffmanError> {
    if src.len() < 16 {
        return Err(HuffmanError::InvalidFormat("read of header failed"));
    }
    Ok(Header {
        magic: u32::from_le_bytes(src[0..4].try_into().expect("slice length checked")),
        permissions: u16::from_le_bytes(src[4..6].try_into().expect("slice length checked")),
        tree_size: u16::from_le_bytes(src[6..8].try_into().expect("slice length checked")),
        file_size: u64::from_le_bytes(src[8..16].try_into().expect("slice length checked")),
    })
}

pub fn crc16_ccitt(data: &[u8]) -> u16 {
    // CRC-16/CCITT-FALSE (same as C):
    // polynomial 0x1021, init 0xFFFF, no reflection, no xorout.
    let mut crc = 0xFFFFu16;
    for &byte in data {
        crc ^= (byte as u16) << 8;
        for _ in 0..8 {
            if (crc & 0x8000) != 0 {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    crc
}

pub fn encode_bytes(
    input: &[u8],
    permissions: u16,
    full_tree: bool,
) -> Result<Vec<u8>, HuffmanError> {
    // Encode steps mirror C:
    //   1) histogram
    //   2) tree build
    //   3) code-table build
    //   4) header + CRC
    //   5) serialized tree
    //   6) bit payload
    let (tree, tree_size) = build_tree(input, full_tree)?;
    let codes = build_codes(&tree);
    let payload = encode_payload(input, &codes);

    let header = Header {
        magic: MAGIC,
        permissions,
        tree_size,
        file_size: input.len() as u64,
    };
    let header_bytes = write_header_bytes(header);
    let crc = crc16_ccitt(&header_bytes);

    let mut tree_bytes = Vec::with_capacity(tree_size as usize);
    dump_tree(&tree, &mut tree_bytes);
    if tree_bytes.len() != tree_size as usize {
        return Err(HuffmanError::InvalidFormat("tree size mismatch"));
    }

    let mut out = Vec::with_capacity(16 + 2 + tree_bytes.len() + payload.len());
    out.extend_from_slice(&header_bytes);
    out.extend_from_slice(&crc.to_le_bytes());
    out.extend_from_slice(&tree_bytes);
    out.extend_from_slice(&payload);
    Ok(out)
}

fn load_tree(saved_tree: &[u8]) -> Result<Node, HuffmanError> {
    // Reconstruct from post-order stream using a stack.
    let mut stack: Vec<Node> = Vec::new();
    let mut i = 0usize;
    while i < saved_tree.len() {
        match saved_tree[i] {
            b'L' => {
                if i + 1 >= saved_tree.len() {
                    return Err(HuffmanError::InvalidFormat("Incorrect tree"));
                }
                stack.push(Node::new_leaf(saved_tree[i + 1], 1));
                i += 2;
            }
            b'I' => {
                let right = stack
                    .pop()
                    .ok_or(HuffmanError::InvalidFormat("Incorrect tree"))?;
                let left = stack
                    .pop()
                    .ok_or(HuffmanError::InvalidFormat("Incorrect tree"))?;
                stack.push(Node::join(left, right));
                i += 1;
            }
            _ => return Err(HuffmanError::InvalidFormat("Incorrect tree")),
        }
    }

    if stack.len() != 1 {
        return Err(HuffmanError::InvalidFormat("Incorrect tree"));
    }
    Ok(stack.pop().expect("length checked"))
}

fn decode_payload(root: &Node, encoded: &[u8], len: u64) -> Result<Vec<u8>, HuffmanError> {
    if root.leaf {
        return Ok(vec![root.symbol; len as usize]);
    }

    let mut out = Vec::with_capacity(len as usize);
    let mut node = root;
    for &byte in encoded {
        for bit in 0..8 {
            if out.len() == len as usize {
                return Ok(out);
            }

            let next = if ((byte >> bit) & 1) == 0 {
                node.left
                    .as_deref()
                    .ok_or(HuffmanError::InvalidFormat("Incorrect tree"))?
            } else {
                node.right
                    .as_deref()
                    .ok_or(HuffmanError::InvalidFormat("Incorrect tree"))?
            };
            node = next;
            if node.leaf {
                out.push(node.symbol);
                node = root;
            }
        }
    }

    if out.len() != len as usize {
        return Err(HuffmanError::InvalidFormat("truncated payload"));
    }
    Ok(out)
}

pub fn decode_bytes(input: &[u8]) -> Result<DecodeResult, HuffmanError> {
    // Decode order follows the C implementation and format contract.
    if input.len() < 16 {
        return Err(HuffmanError::InvalidFormat("Read of header failed"));
    }

    let header_bytes: [u8; 16] = input[0..16].try_into().expect("slice length checked");
    let header = read_header_bytes(&header_bytes)?;
    let magic = header.magic;
    if magic != MAGIC_V1 && magic != MAGIC_V2 {
        return Err(HuffmanError::InvalidFormat("Read of magic number failed"));
    }

    let mut offset = 16usize;
    let mut permissions = header.permissions;
    let mut header_crc_ok = true;
    if magic == MAGIC_V2 {
        if input.len() < offset + 2 {
            return Err(HuffmanError::InvalidFormat("Read of header CRC failed"));
        }
        let stored_crc = u16::from_le_bytes(
            input[offset..offset + 2]
                .try_into()
                .expect("slice length checked"),
        );
        offset += 2;

        let computed_crc = crc16_ccitt(&header_bytes);
        if stored_crc != computed_crc {
            // Keep decoding with safe metadata fallback.
            permissions = FALLBACK_PERMISSIONS;
            header_crc_ok = false;
        }
    }

    let tree_size = header.tree_size as usize;
    if tree_size < MIN_TREE_BYTES || tree_size > MAX_TREE_BYTES || tree_size % 3 != 2 {
        return Err(HuffmanError::InvalidFormat("Incorrect tree"));
    }
    if input.len() < offset + tree_size {
        return Err(HuffmanError::InvalidFormat("Read of tree failed"));
    }
    let saved_tree = &input[offset..offset + tree_size];
    offset += tree_size;

    let tree = load_tree(saved_tree)?;
    let decoded = decode_payload(&tree, &input[offset..], header.file_size)?;
    Ok(DecodeResult {
        data: decoded,
        permissions,
        header_crc_ok,
        format_magic: magic,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crc16_vector() {
        // CRC-16/CCITT-FALSE("123456789") = 0x29B1
        assert_eq!(crc16_ccitt(b"123456789"), 0x29B1);
    }

    #[test]
    fn roundtrip_small() {
        let input = b"aaabbcdefghijklmnopqrstuvwxyz0123456789";
        let encoded = encode_bytes(input, 0o644, false).expect("encode");
        let decoded = decode_bytes(&encoded).expect("decode");
        assert_eq!(decoded.data, input);
        assert_eq!(decoded.permissions, 0o644);
        assert!(decoded.header_crc_ok);
        assert_eq!(decoded.format_magic, MAGIC_V2);
    }
}
