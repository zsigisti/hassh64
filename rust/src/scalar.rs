//! 0..64 bytes: scalar 64x64->128 multiplies. Identical on every target (no AES).
//!
//! All reads go through raw unaligned loads whose bounds are established by the
//! length branch they sit in; each `unsafe` block states which branch guarantees it.

use crate::consts::P;

#[inline(always)]
pub(crate) fn mul(a: u64, b: u64) -> (u64, u64) {
    let r = (a as u128).wrapping_mul(b as u128);
    (r as u64, (r >> 64) as u64)
}

#[inline(always)]
pub(crate) fn fold(a: u64, b: u64) -> u64 {
    let (lo, hi) = mul(a, b);
    lo ^ hi
}

/// Little-endian u64 at `p`. Caller guarantees 8 readable bytes.
#[inline(always)]
unsafe fn r64(p: *const u8) -> u64 {
    u64::from_le(core::ptr::read_unaligned(p as *const u64))
}

/// Little-endian u32 at `p`, zero-extended. Caller guarantees 4 readable bytes.
#[inline(always)]
unsafe fn r32(p: *const u8) -> u64 {
    u32::from_le(core::ptr::read_unaligned(p as *const u32)) as u64
}

/// Exact 4-byte load of bytes 0..3. The empty asm hides the value from the
/// optimizer so LLVM cannot merge it with the adjacent load at p+4 into one
/// 8-byte load. That merged load would start inside (or span) a recent narrow
/// store and miss store forwarding (~12 cycles on current Intel cores), which is
/// exactly the hash-table case of a freshly written integer key. See hassh.h.
#[inline(always)]
unsafe fn r32_head(p: *const u8) -> u64 {
    #[allow(unused_mut)]
    let mut v = r32(p);
    #[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
    core::arch::asm!("/* {0} */", inout(reg) v, options(pure, nomem, nostack, preserves_flags));
    v
}

/// 1..3 bytes at `q` (n must be 1..3): every byte is read at least once.
#[inline(always)]
unsafe fn tail3(q: *const u8, n: usize) -> u64 {
    ((*q as u64) << 16) | ((*q.add(n >> 1) as u64) << 8) | (*q.add(n - 1) as u64)
}

/// First seed key `s` (SPEC "Seed keys"). Depends only on the seed, so a
/// `BuildHasher` computes it once per map.
#[inline(always)]
pub(crate) const fn seed_s(seed: u64) -> u64 {
    let r = ((seed ^ P[0]) as u128).wrapping_mul(P[1] as u128);
    seed ^ (r as u64) ^ ((r >> 64) as u64)
}

/// Second seed key `s2` for a given length.
#[inline(always)]
pub(crate) const fn seed_s2(s: u64, len: u64) -> u64 {
    let r = ((s ^ len ^ P[6]) as u128).wrapping_mul(P[7] as u128);
    (r as u64) ^ ((r >> 64) as u64)
}

#[inline(always)]
fn seed_t(s: u64, len: u64) -> u64 {
    (s ^ P[5] ^ (len << 56)) | 1
}

/// 0..32 bytes.
///
/// Load discipline (must match hassh.h for speed, not for correctness): bytes 0..3
/// are only ever read by loads that START at `p` (4-, 2- or 1-byte), and no other
/// load overlaps them, so a key just written with narrow stores forwards from the
/// store buffer. The 2..3-byte case uses a 16-bit head load for the same reason.
#[inline(always)]
pub(crate) fn small(data: &[u8], seed: u64) -> u64 {
    let s = seed_s(seed);
    small_keyed(data, s, seed_s2(s, data.len() as u64))
}

/// [`small`] with the seed keys already derived: `s = seed_s(seed)` and
/// `s2 = seed_s2(s, data.len())`. Same output.
#[inline(always)]
pub(crate) fn small_keyed(data: &[u8], s: u64, s2: u64) -> u64 {
    let len = data.len();
    debug_assert!(len <= 32);
    let p = data.as_ptr();
    let l = len as u64;
    let t = seed_t(s, l);
    // SAFETY (whole function): every load below is inside the branch whose
    // condition guarantees `len` covers it: offsets + widths never exceed `len`.
    unsafe {
        if len > 16 {
            // 17..32: reads [0,4) [4,8) [8,16) [4 or len-16, +8) [len-8, len).
            let w0 = r32_head(p) | (r32(p.add(4)) << 32);
            let w1 = r64(p.add(8));
            let w2 = r64(p.add(if len < 20 { 4 } else { len - 16 }));
            let w3 = r64(p.add(len - 8));
            let x = fold(w0 ^ P[2] ^ s, w1 ^ P[3] ^ s2);
            let y = fold(w2 ^ P[4] ^ s, w3 ^ P[5] ^ s2);
            let a = fold(x ^ P[6], y ^ P[7] ^ l);
            return fold(a ^ P[4], t);
        }
        let (a, b);
        if len >= 8 {
            a = r32_head(p) | (r32(p.add(4)) << 32);
            b = if len >= 12 {
                r32(p.add(8)) | (r32(p.add(len - 4)) << 32)
            } else if len > 8 {
                tail3(p.add(8), len - 8)
            } else {
                0
            };
        } else if len >= 4 {
            a = r32(p);
            b = if len > 4 { tail3(p.add(4), len - 4) } else { 0 };
        } else if len >= 2 {
            let h = u16::from_le(core::ptr::read_unaligned(p as *const u16)) as u64;
            a = h | if len == 3 { (*p.add(2) as u64) << 16 } else { 0 };
            b = 0;
        } else {
            a = if len == 1 { *p as u64 } else { 0 };
            b = 0;
        }
        // Both product halves feed the second multiply (wyhash structure).
        let (lo, hi) = mul(a ^ P[2] ^ s, b ^ P[3] ^ s2 ^ l);
        fold(lo ^ P[4], hi ^ t)
    }
}

/// 33..64 bytes: four independent multiplies, then one.
#[inline(always)]
pub(crate) fn mid(data: &[u8], seed: u64) -> u64 {
    let s = seed_s(seed);
    mid_keyed(data, s, seed_s2(s, data.len() as u64))
}

/// [`mid`] with the seed keys already derived. Same output.
#[inline(always)]
pub(crate) fn mid_keyed(data: &[u8], s: u64, s2: u64) -> u64 {
    let len = data.len();
    debug_assert!((33..=64).contains(&len));
    let l = len as u64;
    let t = seed_t(s, l);
    let s3 = s ^ s2;
    let p = data.as_ptr();
    // SAFETY: len >= 33, so [0,32) and [len-32, len) are in bounds.
    unsafe {
        let e = p.add(len);
        let x0 = fold(r64(p) ^ P[2] ^ s, r64(p.add(8)) ^ P[3] ^ s2);
        let x1 = fold(r64(p.add(16)) ^ P[4] ^ s, r64(p.add(24)) ^ P[5] ^ s2);
        let x2 = fold(r64(e.sub(32)) ^ P[6] ^ s3, r64(e.sub(24)) ^ P[7] ^ s);
        let x3 = fold(r64(e.sub(16)) ^ P[0] ^ s3, r64(e.sub(8)) ^ P[1] ^ s);
        fold(x0 ^ x2 ^ P[4], x1 ^ x3 ^ t)
    }
}

/// 128-bit seed vector for the AES section: (s, fold(s ^ len ^ P2, P3)).
#[inline(always)]
pub(crate) fn seed_vec(seed: u64, len: u64) -> (u64, u64) {
    let s = seed_s(seed);
    (s, fold(s ^ len ^ P[2], P[3]))
}
