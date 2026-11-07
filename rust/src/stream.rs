//! Incremental hashing with exactly the one-shot result.
//!
//! Inputs under `LONG_MIN` bytes are buffered and hashed in one shot, since
//! their algorithm depends on the total length. From `LONG_MIN` on, the LeMac
//! absorber takes each 128-byte step once a byte after it is known (the final
//! step is aligned to the end), so memory and per-byte cost stay constant.
//!
//! LeMac state layout (shared by every backend, same as the C header): 13
//! slots of 32 bytes, S0..S8, R0, R1, R1 ^ R2, RR; bytes 0..15 of a slot are
//! state 0, bytes 16..31 state 1.

use crate::consts::LONG_MIN;
use crate::portable;
use crate::scalar::seed_s;

const BUF: usize = LONG_MIN;

/// Streaming hassh64: [`update`](Self::update) any number of times with
/// chunks of any size, then [`digest`](Self::digest) gives exactly
/// `hassh64(all bytes, seed)`.
///
/// ```
/// let mut st = hassh64::Hassh64Stream::new(7);
/// st.update(b"hello ");
/// st.update(b"world");
/// assert_eq!(st.digest(), hassh64::hassh64(b"hello world", 7));
/// ```
///
/// About 4.7 KiB; `digest` does not consume the state, so more data can
/// follow. With the `std` feature it also implements `std::io::Write`, so
/// `std::io::copy(&mut file, &mut stream)` hashes a file.
#[derive(Clone)]
pub struct Hassh64Stream {
    seed: u64,
    s: u64,
    total: u64,
    pending: usize,
    lemac: bool,
    a: [u8; 416],
    prev: [u8; 128],
    buf: [u8; BUF],
}

impl core::fmt::Debug for Hassh64Stream {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("Hassh64Stream").field("seed", &self.seed).field("len", &self.total).finish()
    }
}

impl Hassh64Stream {
    /// A new stream hashing with `seed`.
    pub fn new(seed: u64) -> Self {
        Hassh64Stream {
            seed,
            s: seed_s(seed),
            total: 0,
            pending: 0,
            lemac: false,
            a: [0; 416],
            prev: [0; 128],
            buf: [0; BUF],
        }
    }

    /// Bytes hashed so far.
    pub fn len(&self) -> u64 {
        self.total
    }

    /// True if nothing has been hashed yet.
    pub fn is_empty(&self) -> bool {
        self.total == 0
    }

    /// Add `data` to the input.
    pub fn update(&mut self, mut data: &[u8]) {
        self.total += data.len() as u64;
        while !data.is_empty() {
            let n = data.len();
            if self.pending == BUF || (self.lemac && self.pending != 0 && self.pending % 128 == 0 && n > 128) {
                // More input follows, so every buffered whole step is non-final.
                if !self.lemac {
                    self.begin();
                }
                let k = self.pending / 128;
                absorb(&mut self.a, &self.buf[..128 * k], k);
                self.prev.copy_from_slice(&self.buf[128 * k - 128..128 * k]);
                self.pending = 0;
            }
            let n = data.len();
            if self.pending == 0 && n > 128 && (self.lemac || n > BUF) {
                // Absorb straight from the input, keeping 1..128 bytes back.
                let k = (n - 1) / 128;
                if !self.lemac {
                    self.begin();
                }
                absorb(&mut self.a, &data[..128 * k], k);
                self.prev.copy_from_slice(&data[128 * k - 128..128 * k]);
                data = &data[128 * k..];
            }
            let n = data.len();
            let r = self.pending % 128;
            let mut t = BUF - self.pending;
            if self.lemac && r != 0 && n > 256 - r {
                t = 128 - r; // complete the partial step, then go direct
            }
            let t = t.min(n);
            self.buf[self.pending..self.pending + t].copy_from_slice(&data[..t]);
            self.pending += t;
            data = &data[t..];
        }
    }

    /// The hash of everything added so far. Does not change the stream.
    pub fn digest(&self) -> u64 {
        if !self.lemac {
            return crate::hassh64(&self.buf[..self.total as usize], self.seed);
        }
        let mut a = self.a;
        let pn = self.pending; // 1..=BUF
        if pn > 128 {
            let k = (pn - 1) / 128;
            absorb(&mut a, &self.buf[..128 * k], k);
        }
        let mut last = [0u8; 128];
        if pn >= 128 {
            last.copy_from_slice(&self.buf[pn - 128..pn]);
        } else {
            last[..128 - pn].copy_from_slice(&self.prev[pn..]);
            last[128 - pn..].copy_from_slice(&self.buf[..pn]);
        }
        absorb(&mut a, &last, 1); // the final step, aligned to the end
        finish(&a, self.s, self.total)
    }

    fn begin(&mut self) {
        begin(&mut self.a, self.s);
        self.lemac = true;
    }
}

#[cfg(feature = "std")]
impl std::io::Write for Hassh64Stream {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.update(buf);
        Ok(buf.len())
    }
    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

// Backend selection, as for the one-shot 65+ byte path.

#[allow(unreachable_code)]
fn begin(a: &mut [u8; 416], s: u64) {
    #[cfg(all(target_arch = "x86_64", target_feature = "aes", target_feature = "sse4.1"))]
    {
        // SAFETY: the features are enabled at compile time.
        return unsafe { crate::simd::x86::lm_begin(a, s) };
    }
    #[cfg(all(target_arch = "x86_64", not(all(target_feature = "aes", target_feature = "sse4.1")), feature = "std"))]
    {
        if crate::simd::x86::detected() {
            // SAFETY: the CPU reports AES-NI and SSE4.1.
            return unsafe { crate::simd::x86::lm_begin(a, s) };
        }
    }
    #[cfg(all(target_arch = "aarch64", target_endian = "little", target_feature = "aes"))]
    {
        // SAFETY: the feature is enabled at compile time.
        return unsafe { crate::simd::arm::lm_begin(a, s) };
    }
    #[cfg(all(target_arch = "aarch64", target_endian = "little", not(target_feature = "aes"), feature = "std"))]
    {
        if crate::simd::arm::detected() {
            // SAFETY: the CPU reports the AES extension.
            return unsafe { crate::simd::arm::lm_begin(a, s) };
        }
    }
    portable::lm_begin(a, s)
}

#[allow(unreachable_code)]
fn absorb(a: &mut [u8; 416], q: &[u8], n: usize) {
    assert!(q.len() >= 128 * n);
    #[cfg(all(target_arch = "x86_64", feature = "std"))]
    {
        if n > 0 && crate::v3::detected() {
            // SAFETY: the CPU reports AES-NI, SSE4.1, AVX2 and VAES with YMM state saved.
            return unsafe { crate::v3::absorb(a, q, n) };
        }
    }
    #[cfg(all(target_arch = "x86_64", target_feature = "aes", target_feature = "sse4.1"))]
    {
        // SAFETY: the features are enabled at compile time; q holds 128 * n bytes.
        return unsafe { crate::simd::x86::lm_absorb(a, q.as_ptr(), n) };
    }
    #[cfg(all(target_arch = "x86_64", not(all(target_feature = "aes", target_feature = "sse4.1")), feature = "std"))]
    {
        if crate::simd::x86::detected() {
            // SAFETY: the CPU reports AES-NI and SSE4.1; q holds 128 * n bytes.
            return unsafe { crate::simd::x86::lm_absorb(a, q.as_ptr(), n) };
        }
    }
    #[cfg(all(target_arch = "aarch64", target_endian = "little", target_feature = "aes"))]
    {
        // SAFETY: the feature is enabled at compile time; q holds 128 * n bytes.
        return unsafe { crate::simd::arm::lm_absorb(a, q.as_ptr(), n) };
    }
    #[cfg(all(target_arch = "aarch64", target_endian = "little", not(target_feature = "aes"), feature = "std"))]
    {
        if crate::simd::arm::detected() {
            // SAFETY: the CPU reports the AES extension; q holds 128 * n bytes.
            return unsafe { crate::simd::arm::lm_absorb(a, q.as_ptr(), n) };
        }
    }
    portable::lm_absorb(a, q, n)
}

#[allow(unreachable_code)]
fn finish(a: &[u8; 416], s: u64, len: u64) -> u64 {
    #[cfg(all(target_arch = "x86_64", target_feature = "aes", target_feature = "sse4.1"))]
    {
        // SAFETY: the features are enabled at compile time.
        return unsafe { crate::simd::x86::lm_finish(a, s, len) };
    }
    #[cfg(all(target_arch = "x86_64", not(all(target_feature = "aes", target_feature = "sse4.1")), feature = "std"))]
    {
        if crate::simd::x86::detected() {
            // SAFETY: the CPU reports AES-NI and SSE4.1.
            return unsafe { crate::simd::x86::lm_finish(a, s, len) };
        }
    }
    #[cfg(all(target_arch = "aarch64", target_endian = "little", target_feature = "aes"))]
    {
        // SAFETY: the feature is enabled at compile time.
        return unsafe { crate::simd::arm::lm_finish(a, s, len) };
    }
    #[cfg(all(target_arch = "aarch64", target_endian = "little", not(target_feature = "aes"), feature = "std"))]
    {
        if crate::simd::arm::detected() {
            // SAFETY: the CPU reports the AES extension.
            return unsafe { crate::simd::arm::lm_finish(a, s, len) };
        }
    }
    portable::lm_finish(a, s, len)
}
