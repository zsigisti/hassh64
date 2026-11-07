//! # hassh64
//!
//! A fast, seeded, non-cryptographic 64-bit hash, in pure Rust. Output is
//! bit-identical to the C reference (`hassh.h`) and the language-neutral
//! `SPEC.md` on every platform, and is frozen for all 2.x releases. 1.x outputs
//! differ for inputs over 64 bytes (2.0 changed the AES section and added the
//! LeMac absorber for 4096+ bytes); inputs of
//! 0..64 bytes hash the same in 1.x and 2.x.
//!
//! **AI-generated.** The hash, this crate and its tests were written by an AI
//! (Anthropic's Claude, via Claude Code) under human direction and reviewed only
//! by AI agents. hassh64 is **not** a cryptographic hash or MAC and makes **no**
//! claim of HashDoS resistance.
//!
//! ```
//! let h = hassh64::hassh64(b"hello", 0);
//! assert_eq!(h, 0xedfe40b18d046dba);
//!
//! use std::collections::HashMap;
//! let mut m: HashMap<u64, &str, hassh64::Hassh64BuildHasher> = HashMap::default();
//! m.insert(1, "one");
//! ```
//!
//! ## Implementation paths
//!
//! * 0..64 bytes: scalar 64x64->128 multiplies, the same code on every target.
//! * 4096+ bytes, x86_64 with `std`: a VAES + AVX2 LeMac loop (inline
//!   assembly) when the CPU has it, detected at run time.
//! * 65+ bytes, x86_64: AES-NI + SSE4.1 when enabled at compile time
//!   (`-C target-cpu=native` or `-C target-feature=+aes,+sse4.1`); otherwise,
//!   with the `std` feature, picked at run time if the CPU has them.
//! * 65+ bytes, aarch64 (little-endian): ARMv8 AES when the `aes` target feature
//!   is enabled at compile time (default on Apple targets); otherwise, with
//!   `std`, detected at run time.
//! * Everything else (and `no_std` builds without the features): a portable
//!   software AES round. Same output, slower on long inputs.
//!
//! ## Features
//!
//! * `std` (default): run-time CPU detection, `RandomState` and the
//!   `HashMap` / `HashSet` aliases. Without it the crate is `#![no_std]`
//!   and needs no allocator.

#![no_std]
#![warn(missing_docs)]

#[cfg(feature = "std")]
extern crate std;

mod consts;
mod hasher;
mod portable;
mod scalar;
mod simd;
mod stream;
#[cfg(all(target_arch = "x86_64", feature = "std"))]
mod v3;

pub use hasher::{Hassh64BuildHasher, Hassh64Hasher};
pub use stream::Hassh64Stream;
#[cfg(feature = "std")]
pub use hasher::RandomState;

/// `std::collections::HashMap` using [`RandomState`].
#[cfg(feature = "std")]
pub type HashMap<K, V> = std::collections::HashMap<K, V, RandomState>;
/// `std::collections::HashSet` using [`RandomState`].
#[cfg(feature = "std")]
pub type HashSet<T> = std::collections::HashSet<T, RandomState>;

/// Crate version; the output of [`hassh64`] is frozen for all 2.x versions.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");

/// Hash `data` with `seed`.
///
/// Bit-identical to the C `hassh64(data, len, seed)` on every platform and
/// every implementation path.
#[inline]
pub fn hassh64(data: &[u8], seed: u64) -> u64 {
    let len = data.len();
    if len <= 32 {
        scalar::small(data, seed)
    } else if len <= 64 {
        scalar::mid(data, seed)
    } else {
        large(data, seed)
    }
}

/// 65+ bytes on the hardware AES path, if this build/CPU has one.
#[inline]
fn accel_large(data: &[u8], seed: u64) -> Option<u64> {
    debug_assert!(data.len() > 64);
    #[cfg(all(target_arch = "x86_64", feature = "std"))]
    {
        if data.len() >= consts::LONG_MIN && v3::detected() {
            // SAFETY: the CPU reports AES-NI, SSE4.1, AVX2 and VAES with YMM state saved.
            return Some(unsafe { v3::long(data, seed) });
        }
    }
    #[cfg(all(target_arch = "x86_64", target_feature = "aes", target_feature = "sse4.1"))]
    {
        // SAFETY: the features are enabled at compile time; len > 64.
        return Some(unsafe { simd::x86::large(data, seed) });
    }
    #[cfg(all(
        target_arch = "x86_64",
        not(all(target_feature = "aes", target_feature = "sse4.1")),
        feature = "std"
    ))]
    {
        if simd::x86::detected() {
            // SAFETY: the CPU reports AES-NI and SSE4.1; len > 64.
            return Some(unsafe { simd::x86::large(data, seed) });
        }
    }
    #[cfg(all(target_arch = "aarch64", target_endian = "little", target_feature = "aes"))]
    {
        // SAFETY: the feature is enabled at compile time; len > 64.
        return Some(unsafe { simd::arm::large(data, seed) });
    }
    #[cfg(all(
        target_arch = "aarch64",
        target_endian = "little",
        not(target_feature = "aes"),
        feature = "std"
    ))]
    {
        if simd::arm::detected() {
            // SAFETY: the CPU reports the AES extension; len > 64.
            return Some(unsafe { simd::arm::large(data, seed) });
        }
    }
    #[allow(unreachable_code)]
    {
        let _ = (data, seed);
        None
    }
}

#[inline]
fn large(data: &[u8], seed: u64) -> u64 {
    match accel_large(data, seed) {
        Some(h) => h,
        None => portable::large(data, seed),
    }
}

/// Internal hooks for the test suite. Not part of the public API; may change in
/// any release.
#[doc(hidden)]
pub mod __private {
    /// hassh64 forced onto the portable (software AES) path.
    pub fn hassh64_portable(data: &[u8], seed: u64) -> u64 {
        if data.len() <= 64 {
            crate::hassh64(data, seed)
        } else {
            crate::portable::large(data, seed)
        }
    }

    /// hassh64 on the hardware AES path, or `None` if this build/CPU has none.
    pub fn hassh64_accel(data: &[u8], seed: u64) -> Option<u64> {
        if data.len() <= 64 {
            Some(crate::hassh64(data, seed))
        } else {
            crate::accel_large(data, seed)
        }
    }

    /// Name of the 65+-byte path `hassh64` uses here.
    pub fn path_name() -> &'static str {
        let probe = [0u8; 65];
        if crate::accel_large(&probe, 0).is_none() {
            "portable"
        } else if cfg!(target_arch = "x86_64") {
            "x86_64 AES-NI"
        } else {
            "aarch64 AES"
        }
    }

    /// One software AES round (for the FIPS-197 / Intel AESENC example test).
    pub fn aesenc_portable(x: &[u8; 16], k: &[u8; 16]) -> [u8; 16] {
        let blk = |b: &[u8; 16]| {
            let mut lo = [0u8; 8];
            let mut hi = [0u8; 8];
            lo.copy_from_slice(&b[..8]);
            hi.copy_from_slice(&b[8..]);
            [u64::from_le_bytes(lo), u64::from_le_bytes(hi)]
        };
        let o = crate::portable::aesenc(&blk(x), &blk(k));
        let mut r = [0u8; 16];
        r[..8].copy_from_slice(&o[0].to_le_bytes());
        r[8..].copy_from_slice(&o[1].to_le_bytes());
        r
    }
}
