//! Portable 65+-byte path: software AES round (one 1 KiB T-table). The reference
//! the hardware paths are tested against; used on targets without AES instructions.

use crate::consts::{FIN_KEY0, FIN_ROUNDS, INIT_KEY0, INIT_KEY1, K, L, LONG_MIN, ROUNDS, ROUND_KEY0};
use crate::scalar::{len_vec, seed_s, seed_vec};

/// A block as two little-endian 64-bit halves (bytes 0..7, 8..15). Column c
/// (bytes 4c..4c+3) is 32 bits of half c / 2.
type Blk = [u64; 2];

const TE: [u32; 256] = [
    0xa56363c6, 0x847c7cf8, 0x997777ee, 0x8d7b7bf6, 0x0df2f2ff, 0xbd6b6bd6,
    0xb16f6fde, 0x54c5c591, 0x50303060, 0x03010102, 0xa96767ce, 0x7d2b2b56,
    0x19fefee7, 0x62d7d7b5, 0xe6abab4d, 0x9a7676ec, 0x45caca8f, 0x9d82821f,
    0x40c9c989, 0x877d7dfa, 0x15fafaef, 0xeb5959b2, 0xc947478e, 0x0bf0f0fb,
    0xecadad41, 0x67d4d4b3, 0xfda2a25f, 0xeaafaf45, 0xbf9c9c23, 0xf7a4a453,
    0x967272e4, 0x5bc0c09b, 0xc2b7b775, 0x1cfdfde1, 0xae93933d, 0x6a26264c,
    0x5a36366c, 0x413f3f7e, 0x02f7f7f5, 0x4fcccc83, 0x5c343468, 0xf4a5a551,
    0x34e5e5d1, 0x08f1f1f9, 0x937171e2, 0x73d8d8ab, 0x53313162, 0x3f15152a,
    0x0c040408, 0x52c7c795, 0x65232346, 0x5ec3c39d, 0x28181830, 0xa1969637,
    0x0f05050a, 0xb59a9a2f, 0x0907070e, 0x36121224, 0x9b80801b, 0x3de2e2df,
    0x26ebebcd, 0x6927274e, 0xcdb2b27f, 0x9f7575ea, 0x1b090912, 0x9e83831d,
    0x742c2c58, 0x2e1a1a34, 0x2d1b1b36, 0xb26e6edc, 0xee5a5ab4, 0xfba0a05b,
    0xf65252a4, 0x4d3b3b76, 0x61d6d6b7, 0xceb3b37d, 0x7b292952, 0x3ee3e3dd,
    0x712f2f5e, 0x97848413, 0xf55353a6, 0x68d1d1b9, 0x00000000, 0x2cededc1,
    0x60202040, 0x1ffcfce3, 0xc8b1b179, 0xed5b5bb6, 0xbe6a6ad4, 0x46cbcb8d,
    0xd9bebe67, 0x4b393972, 0xde4a4a94, 0xd44c4c98, 0xe85858b0, 0x4acfcf85,
    0x6bd0d0bb, 0x2aefefc5, 0xe5aaaa4f, 0x16fbfbed, 0xc5434386, 0xd74d4d9a,
    0x55333366, 0x94858511, 0xcf45458a, 0x10f9f9e9, 0x06020204, 0x817f7ffe,
    0xf05050a0, 0x443c3c78, 0xba9f9f25, 0xe3a8a84b, 0xf35151a2, 0xfea3a35d,
    0xc0404080, 0x8a8f8f05, 0xad92923f, 0xbc9d9d21, 0x48383870, 0x04f5f5f1,
    0xdfbcbc63, 0xc1b6b677, 0x75dadaaf, 0x63212142, 0x30101020, 0x1affffe5,
    0x0ef3f3fd, 0x6dd2d2bf, 0x4ccdcd81, 0x140c0c18, 0x35131326, 0x2fececc3,
    0xe15f5fbe, 0xa2979735, 0xcc444488, 0x3917172e, 0x57c4c493, 0xf2a7a755,
    0x827e7efc, 0x473d3d7a, 0xac6464c8, 0xe75d5dba, 0x2b191932, 0x957373e6,
    0xa06060c0, 0x98818119, 0xd14f4f9e, 0x7fdcdca3, 0x66222244, 0x7e2a2a54,
    0xab90903b, 0x8388880b, 0xca46468c, 0x29eeeec7, 0xd3b8b86b, 0x3c141428,
    0x79dedea7, 0xe25e5ebc, 0x1d0b0b16, 0x76dbdbad, 0x3be0e0db, 0x56323264,
    0x4e3a3a74, 0x1e0a0a14, 0xdb494992, 0x0a06060c, 0x6c242448, 0xe45c5cb8,
    0x5dc2c29f, 0x6ed3d3bd, 0xefacac43, 0xa66262c4, 0xa8919139, 0xa4959531,
    0x37e4e4d3, 0x8b7979f2, 0x32e7e7d5, 0x43c8c88b, 0x5937376e, 0xb76d6dda,
    0x8c8d8d01, 0x64d5d5b1, 0xd24e4e9c, 0xe0a9a949, 0xb46c6cd8, 0xfa5656ac,
    0x07f4f4f3, 0x25eaeacf, 0xaf6565ca, 0x8e7a7af4, 0xe9aeae47, 0x18080810,
    0xd5baba6f, 0x887878f0, 0x6f25254a, 0x722e2e5c, 0x241c1c38, 0xf1a6a657,
    0xc7b4b473, 0x51c6c697, 0x23e8e8cb, 0x7cdddda1, 0x9c7474e8, 0x211f1f3e,
    0xdd4b4b96, 0xdcbdbd61, 0x868b8b0d, 0x858a8a0f, 0x907070e0, 0x423e3e7c,
    0xc4b5b571, 0xaa6666cc, 0xd8484890, 0x05030306, 0x01f6f6f7, 0x120e0e1c,
    0xa36161c2, 0x5f35356a, 0xf95757ae, 0xd0b9b969, 0x91868617, 0x58c1c199,
    0x271d1d3a, 0xb99e9e27, 0x38e1e1d9, 0x13f8f8eb, 0xb398982b, 0x33111122,
    0xbb6969d2, 0x70d9d9a9, 0x898e8e07, 0xa7949433, 0xb69b9b2d, 0x221e1e3c,
    0x92878715, 0x20e9e9c9, 0x49cece87, 0xff5555aa, 0x78282850, 0x7adfdfa5,
    0x8f8c8c03, 0xf8a1a159, 0x80898909, 0x170d0d1a, 0xdabfbf65, 0x31e6e6d7,
    0xc6424284, 0xb86868d0, 0xc3414182, 0xb0999929, 0x772d2d5a, 0x110f0f1e,
    0xcbb0b07b, 0xfc5454a8, 0xd6bbbb6d, 0x3a16162c,
];

/// One AES encryption round, identical to x86 AESENC:
/// MixColumns(ShiftRows(SubBytes(x))) ^ k. Byte 4c+r is row r, column c.
/// `TE[v]` is the MixColumns column (2,1,1,3)·S(v), row 0 in the low byte;
/// row r uses it rotated left by 8r bits.
#[inline(always)]
pub(crate) fn aesenc(x: &Blk, k: &Blk) -> Blk {
    let (a, b) = (x[0] as u32, (x[0] >> 32) as u32);
    let (c, d) = (x[1] as u32, (x[1] >> 32) as u32);
    #[inline(always)]
    fn col(w0: u32, w1: u32, w2: u32, w3: u32) -> u32 {
        TE[(w0 & 0xff) as usize]
            ^ TE[((w1 >> 8) & 0xff) as usize].rotate_left(8)
            ^ TE[((w2 >> 16) & 0xff) as usize].rotate_left(16)
            ^ TE[(w3 >> 24) as usize].rotate_left(24)
    }
    [
        (col(a, b, c, d) as u64 | (col(b, c, d, a) as u64) << 32) ^ k[0],
        (col(c, d, a, b) as u64 | (col(d, a, b, c) as u64) << 32) ^ k[1],
    ]
}

#[inline(always)]
fn set(lo: u64, hi: u64) -> Blk {
    [lo, hi]
}

#[inline(always)]
fn key(i: usize) -> Blk {
    set(K[2 * i], K[2 * i + 1])
}

#[inline(always)]
fn xor(a: Blk, b: &Blk) -> Blk {
    [a[0] ^ b[0], a[1] ^ b[1]]
}

#[inline(always)]
fn ld(p: &[u8], off: usize) -> Blk {
    let b = &p[off..off + 16];
    [
        u64::from_le_bytes([b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]]),
        u64::from_le_bytes([b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]]),
    ]
}

#[inline]
fn perm(mut x: Blk) -> Blk {
    for r in 0..ROUNDS {
        x = aesenc(&x, &key(ROUND_KEY0 + r));
    }
    x
}

#[inline]
fn out(mut x: Blk) -> u64 {
    for r in 0..FIN_ROUNDS {
        x = aesenc(&x, &key(FIN_KEY0 + r));
    }
    x[0] ^ x[1]
}

#[inline(always)]
fn lkey(w: usize) -> Blk {
    set(L[2 * w], L[2 * w + 1])
}

/// One LeMac step (Bariant, David, Leurent, ToSC 2024; corrected message
/// schedule): `s` = S0..S8, `r` = RR, R0, R1, R2. The message words are the
/// round keys.
#[inline]
fn lm_step(s: &mut [Blk; 9], r: &mut [Blk; 4], m: [Blk; 4]) {
    let s0 = xor(xor(s[0], &s[8]), &m[2]);
    s[8] = aesenc(&s[7], &m[3]);
    s[7] = aesenc(&s[6], &m[1]);
    s[6] = aesenc(&s[5], &m[1]);
    s[5] = aesenc(&s[4], &m[0]);
    s[4] = aesenc(&s[3], &m[0]);
    s[3] = aesenc(&s[2], &xor(r[2], &r[3]));
    s[2] = aesenc(&s[1], &m[3]);
    s[1] = aesenc(&s[0], &m[3]);
    s[0] = s0;
    r[3] = r[2];
    r[2] = r[1];
    r[1] = xor(r[0], &m[1]);
    r[0] = m[2];
}

/// 4096+ bytes, portable: two LeMac states (see SPEC.md).
fn long(p: &[u8], seed: u64) -> u64 {
    let len = p.len();
    let s = seed_s(seed);
    let (svlo, svhi) = seed_vec(s);
    let sv = set(svlo, svhi);
    let (lvlo, lvhi) = len_vec(s, len as u64);
    let mut x = aesenc(&aesenc(&set(lvlo, lvhi), &key(INIT_KEY0)), &key(INIT_KEY1));
    let nsteps = (len + 127) / 128;
    let z: Blk = [0; 2];
    for j in 0..2 {
        let mut st = [z; 9];
        for (i, w) in st.iter_mut().enumerate() {
            *w = aesenc(&aesenc(&xor(lkey(9 * j + i), &sv), &key(INIT_KEY0)), &key(INIT_KEY1));
        }
        let mut r = [z; 4];
        for k in 0..nsteps {
            let q = if k + 1 < nsteps { 128 * k } else { len - 128 } + 16 * j;
            lm_step(&mut st, &mut r, [ld(p, q), ld(p, q + 32), ld(p, q + 64), ld(p, q + 96)]);
        }
        for _ in 0..4 {
            lm_step(&mut st, &mut r, [z; 4]);
        }
        for (i, w) in st.iter().enumerate() {
            let mut y = xor(*w, &lkey(18 + 9 * j + i));
            for rr in 0..4 {
                y = aesenc(&y, &key(ROUND_KEY0 + rr));
            }
            x = xor(x, &y);
        }
    }
    out(x)
}

/// 65+ bytes, portable.
pub(crate) fn large(p: &[u8], seed: u64) -> u64 {
    let len = p.len();
    debug_assert!(len > 64);
    if len >= LONG_MIN {
        return long(p, seed);
    }
    let s = seed_s(seed);
    let (svlo, svhi) = seed_vec(s);
    let sv = set(svlo, svhi);
    let lanes = if len <= 128 { 8 } else { 16 };
    let mut acc: [Blk; 16] = [[0; 2]; 16];
    for (i, a) in acc.iter_mut().enumerate().take(lanes) {
        *a = aesenc(&aesenc(&xor(key(i), &sv), &key(INIT_KEY0)), &key(INIT_KEY1));
    }
    if len <= 256 {
        // First half of the lanes read from the start, second half from the end.
        let half = lanes / 2;
        for (i, a) in acc.iter_mut().enumerate().take(lanes) {
            let off = if i < half { 16 * i } else { len - 16 * (lanes - i) };
            *a = perm(xor(*a, &ld(p, off)));
        }
    } else {
        // 256-byte stripes, the final stripe aligned to the end of the input.
        let mut off = 0usize;
        loop {
            if off + 256 > len {
                off = len - 256;
            }
            for (i, a) in acc.iter_mut().enumerate() {
                *a = perm(xor(*a, &ld(p, off + 16 * i)));
            }
            if off + 256 >= len {
                break;
            }
            off += 256;
        }
    }
    let mut x = acc[0];
    for a in acc.iter().take(lanes).skip(1) {
        x = xor(x, a);
    }
    let (lvlo, lvhi) = len_vec(s, len as u64);
    let lv = aesenc(&aesenc(&set(lvlo, lvhi), &key(INIT_KEY0)), &key(INIT_KEY1));
    out(xor(x, &lv))
}
