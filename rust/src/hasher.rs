//! `core::hash::Hasher` / `BuildHasher` adapters.

use core::fmt;
use core::hash::{BuildHasher, Hasher};
use core::mem::MaybeUninit;

use crate::hassh64;
use crate::consts::P;
use crate::scalar::{mid_keyed, mul, seed_s, seed_s2, small_keyed};

/// Seed keys that depend only on the seed, derived once per seed.
///
/// `s2_8` is the second key for an 8-byte input (a `u64` or `usize` key, the
/// common case), so that case is two dependent multiplies. Kept to three words:
/// the hasher is copied on every map operation, and a larger struct measured
/// 4x slower in a HashMap benchmark.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
struct Keys {
    seed: u64,
    s: u64,
    s2_8: u64,
}

impl Keys {
    #[inline]
    const fn new(seed: u64) -> Self {
        let s = seed_s(seed);
        Self { seed, s, s2_8: seed_s2(s, 8) }
    }
}

/// Inline buffer size. Keys whose bytes total at most this many are hashed with
/// one `hassh64` call in `finish`.
const BUF: usize = 64;

/// A streaming [`Hasher`] built on [`hassh64`](crate::hassh64).
///
/// hassh64 is a one-shot function, so the hasher buffers. Writes are appended to a
/// 64-byte inline buffer; `finish` returns `hassh64(buffer, state)`. When a write
/// does not fit, the buffer is topped up to 64 bytes and compressed into the state
/// with `state = hassh64(buffer, state)` (chaining through the seed); a remaining
/// part longer than 64 bytes is compressed the same way in one call, and anything
/// shorter starts a new buffer.
///
/// Consequences:
///
/// * A hasher that received at most 64 bytes in total returns exactly
///   `hassh64(those bytes, seed)`, however they were split. In particular a
///   single `write(b)` with `b.len() <= 64` gives `hassh64(b, seed)`.
/// * For longer inputs the result depends on how the bytes were split into
///   `write` calls and is **not** equal to `hassh64` of the concatenation.
///   That is allowed by the `Hasher` contract (std's `Hash` impls add length
///   prefixes or terminators), but do not compare `Hasher` output with
///   [`hassh64`](crate::hassh64) output, or across different write patterns.
/// * Integer writes (`write_u8` .. `write_u128`, `write_usize`, and the signed
///   ones) append the little-endian bytes, `usize`/`isize` widened to 64 bits,
///   so `Hasher` output is the same on every platform. A `u64` key in a
///   `HashMap` costs one 8-byte store and one 8-byte hassh64.
///
/// `finish` does not consume or reset the state (per the `Hasher` contract).
#[derive(Clone, Copy)]
pub struct Hassh64Hasher {
    keys: Keys,
    len: usize,
    buf: MaybeUninit<[u8; BUF]>,
}

impl Hassh64Hasher {
    /// A hasher with the given seed.
    #[inline]
    pub const fn with_seed(seed: u64) -> Self {
        Self::with_keys(Keys::new(seed))
    }

    #[inline(always)]
    const fn with_keys(keys: Keys) -> Self {
        Self { keys, len: 0, buf: MaybeUninit::uninit() }
    }

    #[inline(always)]
    fn buf_ptr(&mut self) -> *mut u8 {
        self.buf.as_mut_ptr() as *mut u8
    }

    /// The bytes written since the last compression.
    #[inline(always)]
    fn buffered(&self) -> &[u8] {
        // SAFETY: the first `len` bytes (len <= BUF) have always been written.
        unsafe { core::slice::from_raw_parts(self.buf.as_ptr() as *const u8, self.len) }
    }

    /// Append a fixed-size little-endian integer.
    #[inline(always)]
    fn push<const N: usize>(&mut self, bytes: [u8; N]) {
        if self.len + N <= BUF {
            // SAFETY: len + N <= BUF, so the write is in bounds.
            unsafe { core::ptr::write_unaligned(self.buf_ptr().add(self.len) as *mut [u8; N], bytes) };
            self.len += N;
        } else {
            self.write_slow(&bytes);
        }
    }

    #[cold]
    #[inline(never)]
    fn write_slow(&mut self, mut bytes: &[u8]) {
        if self.len > 0 {
            let take = BUF - self.len;
            // SAFETY: len + take == BUF; caller guarantees bytes.len() > take.
            unsafe {
                core::ptr::copy_nonoverlapping(bytes.as_ptr(), self.buf_ptr().add(self.len), take)
            };
            self.len = BUF;
            self.keys = Keys::new(hassh64(self.buffered(), self.keys.seed));
            self.len = 0;
            bytes = &bytes[take..];
        }
        if bytes.len() > BUF {
            self.keys = Keys::new(hassh64(bytes, self.keys.seed));
        } else {
            // SAFETY: len == 0 and bytes.len() <= BUF.
            unsafe { core::ptr::copy_nonoverlapping(bytes.as_ptr(), self.buf_ptr(), bytes.len()) };
            self.len = bytes.len();
        }
    }
}

impl Default for Hassh64Hasher {
    /// Seed 0.
    #[inline]
    fn default() -> Self {
        Self::with_seed(0)
    }
}

impl fmt::Debug for Hassh64Hasher {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Hassh64Hasher").field("buffered", &self.len).finish_non_exhaustive()
    }
}

impl Hasher for Hassh64Hasher {
    /// `hassh64(buffered bytes, state)`, using the seed keys derived when the
    /// seed was set. The buffer never holds more than 64 bytes, so only the
    /// scalar paths are reachable.
    #[inline]
    fn finish(&self) -> u64 {
        let data = self.buffered();
        let k = &self.keys;
        let n = data.len();
        if n == 8 {
            // SAFETY: n == 8 bytes are buffered. The hasher's own buffer was just
            // written whole, so one 8-byte load forwards cleanly.
            let a = u64::from_le(unsafe { core::ptr::read_unaligned(data.as_ptr() as *const u64) });
            // SPEC "Length 0 to 16" with len = 8, a = the key, b = 0.
            let t8 = (k.s ^ P[5] ^ (8u64 << 56)) | 1;
            let (lo, hi) = mul(a ^ P[2] ^ k.s, P[3] ^ k.s2_8 ^ 8);
            let r = ((lo ^ P[4]) as u128).wrapping_mul((hi ^ t8) as u128);
            (r as u64) ^ ((r >> 64) as u64)
        } else if n <= 32 {
            small_keyed(data, k.s, seed_s2(k.s, n as u64))
        } else {
            mid_keyed(data, k.s, seed_s2(k.s, n as u64))
        }
    }

    #[inline]
    fn write(&mut self, bytes: &[u8]) {
        if self.len + bytes.len() <= BUF {
            // SAFETY: the copy ends at len + bytes.len() <= BUF.
            unsafe {
                core::ptr::copy_nonoverlapping(bytes.as_ptr(), self.buf_ptr().add(self.len), bytes.len())
            };
            self.len += bytes.len();
        } else {
            self.write_slow(bytes);
        }
    }

    #[inline]
    fn write_u8(&mut self, i: u8) {
        self.push(i.to_le_bytes());
    }
    #[inline]
    fn write_u16(&mut self, i: u16) {
        self.push(i.to_le_bytes());
    }
    #[inline]
    fn write_u32(&mut self, i: u32) {
        self.push(i.to_le_bytes());
    }
    #[inline]
    fn write_u64(&mut self, i: u64) {
        self.push(i.to_le_bytes());
    }
    #[inline]
    fn write_u128(&mut self, i: u128) {
        self.push(i.to_le_bytes());
    }
    #[inline]
    fn write_usize(&mut self, i: usize) {
        self.push((i as u64).to_le_bytes());
    }
    #[inline]
    fn write_i8(&mut self, i: i8) {
        self.write_u8(i as u8);
    }
    #[inline]
    fn write_i16(&mut self, i: i16) {
        self.write_u16(i as u16);
    }
    #[inline]
    fn write_i32(&mut self, i: i32) {
        self.write_u32(i as u32);
    }
    #[inline]
    fn write_i64(&mut self, i: i64) {
        self.write_u64(i as u64);
    }
    #[inline]
    fn write_i128(&mut self, i: i128) {
        self.write_u128(i as u128);
    }
    #[inline]
    fn write_isize(&mut self, i: isize) {
        self.write_u64(i as i64 as u64);
    }
}

/// A [`BuildHasher`] with a fixed seed. Deterministic across runs and platforms;
/// `Default` uses seed 0. Use [`RandomState`](crate::RandomState) (feature `std`)
/// for a per-process random seed when keys may come from an adversary (bearing in
/// mind that hassh64 makes no HashDoS-resistance claim).
///
/// The seed keys are derived once here, not once per hashed key.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Hassh64BuildHasher {
    keys: Keys,
}

impl Hassh64BuildHasher {
    /// A builder whose hashers all start from `seed`.
    #[inline]
    pub const fn with_seed(seed: u64) -> Self {
        Self { keys: Keys::new(seed) }
    }

    /// The seed.
    #[inline]
    pub const fn seed(&self) -> u64 {
        self.keys.seed
    }
}

impl Default for Hassh64BuildHasher {
    /// Seed 0.
    #[inline]
    fn default() -> Self {
        Self::with_seed(0)
    }
}

impl BuildHasher for Hassh64BuildHasher {
    type Hasher = Hassh64Hasher;
    #[inline]
    fn build_hasher(&self) -> Hassh64Hasher {
        Hassh64Hasher::with_keys(self.keys)
    }
}

#[cfg(feature = "std")]
mod random {
    use super::{Hassh64BuildHasher, Hassh64Hasher};
    use core::hash::{BuildHasher, Hasher};
    use core::sync::atomic::{AtomicU64, Ordering};

    /// A [`BuildHasher`] with a random seed, like std's `RandomState`.
    ///
    /// The seed comes from std's own `RandomState` (keyed from OS randomness once
    /// per thread, then varied per instance), mixed with a process-wide counter,
    /// so it needs no extra dependency. Each `RandomState::new()` gets a different
    /// seed; clones share it.
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct RandomState {
        inner: Hassh64BuildHasher,
    }

    impl RandomState {
        /// A builder with a fresh random seed.
        pub fn new() -> Self {
            static COUNTER: AtomicU64 = AtomicU64::new(0);
            let mut h = std::collections::hash_map::RandomState::new().build_hasher();
            h.write_u64(COUNTER.fetch_add(1, Ordering::Relaxed));
            Self { inner: Hassh64BuildHasher::with_seed(h.finish()) }
        }

        /// The seed this builder uses.
        #[inline]
        pub fn seed(&self) -> u64 {
            self.inner.seed()
        }
    }

    impl Default for RandomState {
        #[inline]
        fn default() -> Self {
            Self::new()
        }
    }

    impl BuildHasher for RandomState {
        type Hasher = Hassh64Hasher;
        #[inline]
        fn build_hasher(&self) -> Hassh64Hasher {
            self.inner.build_hasher()
        }
    }
}

#[cfg(feature = "std")]
pub use random::RandomState;
