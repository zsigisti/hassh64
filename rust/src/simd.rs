//! 65+-byte path on CPUs with a hardware AES round on 128-bit vectors.
//!
//! `simd_core!` expands the algorithm once per ISA. Each ISA module supplies
//! `V`, `ld`, `xor`, `enc` (one round == x86 AESENC), `key`, `set` and `fold`,
//! all with the same `#[target_feature]` attributes, so everything inlines into
//! the entry point even when the crate is built without those features.

#[allow(unused_macros)]
macro_rules! simd_core {
    ($(#[$tf:meta])*) => {
        use crate::consts::{FIN_KEY0, FIN_ROUNDS, INIT_KEY0, INIT_KEY1, ROUNDS, ROUND_KEY0};
        use crate::scalar::{len_vec, seed_s, seed_vec};

        $(#[$tf])*
        #[inline]
        unsafe fn perm(mut x: V) -> V {
            for r in 0..ROUNDS {
                x = enc(x, key(ROUND_KEY0 + r));
            }
            x
        }

        $(#[$tf])*
        #[inline]
        unsafe fn out(mut x: V) -> u64 {
            for r in 0..FIN_ROUNDS {
                x = enc(x, key(FIN_KEY0 + r));
            }
            fold(x)
        }

        $(#[$tf])*
        #[inline]
        unsafe fn init(i: usize, sv: V) -> V {
            enc(enc(xor(key(i), sv), key(INIT_KEY0)), key(INIT_KEY1))
        }

        /// 65+ bytes.
        ///
        /// # Safety
        /// The CPU must support the target features above, and `data.len() > 64`.
        $(#[$tf])*
        #[inline]
        pub(crate) unsafe fn large(data: &[u8], seed: u64) -> u64 {
            let len = data.len();
            debug_assert!(len > 64);
            let s = seed_s(seed);
            let (lo, hi) = seed_vec(s);
            let sv = set(lo, hi);
            // Length vector: independent of the lanes, so it runs in parallel.
            let (llo, lhi) = len_vec(s, len as u64);
            let lv = enc(enc(set(llo, lhi), key(INIT_KEY0)), key(INIT_KEY1));
            let p = data.as_ptr();
            let e = p.add(len);
            if len <= 128 {
                // 8 lanes: 4 from the head, 4 from the tail (overlapping).
                let x = xor(
                    xor(perm(xor(init(0, sv), ld(p))), perm(xor(init(1, sv), ld(p.add(16))))),
                    xor(perm(xor(init(2, sv), ld(p.add(32)))), perm(xor(init(3, sv), ld(p.add(48))))),
                );
                let y = xor(
                    xor(perm(xor(init(4, sv), ld(e.sub(64)))), perm(xor(init(5, sv), ld(e.sub(48))))),
                    xor(perm(xor(init(6, sv), ld(e.sub(32)))), perm(xor(init(7, sv), ld(e.sub(16))))),
                );
                return out(xor(xor(x, y), lv));
            }
            let mut a: [V; 16] = [
                init(0, sv), init(1, sv), init(2, sv), init(3, sv),
                init(4, sv), init(5, sv), init(6, sv), init(7, sv),
                init(8, sv), init(9, sv), init(10, sv), init(11, sv),
                init(12, sv), init(13, sv), init(14, sv), init(15, sv),
            ];
            if len <= 256 {
                let q = e.sub(128);
                for i in 0..8 {
                    a[i] = perm(xor(a[i], ld(p.add(16 * i))));
                }
                for i in 8..16 {
                    a[i] = perm(xor(a[i], ld(q.add(16 * (i - 8)))));
                }
            } else {
                // 256-byte stripes, the last one aligned to the end of the input.
                let last = e.sub(256);
                let mut off = 0usize;
                loop {
                    let q = if off < len - 256 { p.add(off) } else { last };
                    for (i, ai) in a.iter_mut().enumerate() {
                        *ai = perm(xor(*ai, ld(q.add(16 * i))));
                    }
                    if q == last {
                        break;
                    }
                    off += 256;
                }
            }
            let mut x = a[0];
            for ai in &a[1..] {
                x = xor(x, *ai);
            }
            out(xor(x, lv))
        }
    };
}

#[cfg(all(
    target_arch = "x86_64",
    any(all(target_feature = "aes", target_feature = "sse4.1"), feature = "std")
))]
pub(crate) mod x86 {
    use crate::consts::K;
    use core::arch::x86_64::*;

    type V = __m128i;

    #[target_feature(enable = "aes,sse4.1")]
    #[inline]
    unsafe fn ld(p: *const u8) -> V {
        _mm_loadu_si128(p as *const V)
    }
    #[target_feature(enable = "aes,sse4.1")]
    #[inline]
    unsafe fn xor(a: V, b: V) -> V {
        _mm_xor_si128(a, b)
    }
    #[target_feature(enable = "aes,sse4.1")]
    #[inline]
    unsafe fn enc(x: V, k: V) -> V {
        _mm_aesenc_si128(x, k)
    }
    /// Key i: K[2i] (lo), K[2i+1] (hi). x86_64 is little-endian, so the raw
    /// load equals the little-endian block.
    #[target_feature(enable = "aes,sse4.1")]
    #[inline]
    unsafe fn key(i: usize) -> V {
        _mm_loadu_si128(K.as_ptr().add(2 * i) as *const V)
    }
    #[target_feature(enable = "aes,sse4.1")]
    #[inline]
    unsafe fn set(lo: u64, hi: u64) -> V {
        _mm_set_epi64x(hi as i64, lo as i64)
    }
    #[target_feature(enable = "aes,sse4.1")]
    #[inline]
    unsafe fn fold(x: V) -> u64 {
        (_mm_cvtsi128_si64(x) as u64) ^ (_mm_extract_epi64(x, 1) as u64)
    }

    simd_core!(#[target_feature(enable = "aes,sse4.1")]);

    /// Run-time check (std only). `is_x86_feature_detected!` caches the CPUID
    /// result in std, so this is an atomic load and a bit test.
    #[cfg(all(feature = "std", not(all(target_feature = "aes", target_feature = "sse4.1"))))]
    #[inline]
    pub(crate) fn detected() -> bool {
        std::is_x86_feature_detected!("aes") && std::is_x86_feature_detected!("sse4.1")
    }
}

#[cfg(all(
    target_arch = "aarch64",
    target_endian = "little",
    any(target_feature = "aes", feature = "std")
))]
pub(crate) mod arm {
    use crate::consts::K;
    use core::arch::aarch64::*;

    type V = uint8x16_t;

    #[target_feature(enable = "neon,aes")]
    #[inline]
    unsafe fn ld(p: *const u8) -> V {
        vld1q_u8(p)
    }
    #[target_feature(enable = "neon,aes")]
    #[inline]
    unsafe fn xor(a: V, b: V) -> V {
        veorq_u8(a, b)
    }
    /// AESE(x, k) = ShiftRows(SubBytes(x ^ k)); AESMC = MixColumns. With a zero
    /// AESE key, AESMC(AESE(x, 0)) ^ k is exactly x86 AESENC(x, k).
    #[target_feature(enable = "neon,aes")]
    #[inline]
    unsafe fn enc(x: V, k: V) -> V {
        veorq_u8(vaesmcq_u8(vaeseq_u8(x, vdupq_n_u8(0))), k)
    }
    #[target_feature(enable = "neon,aes")]
    #[inline]
    unsafe fn key(i: usize) -> V {
        vld1q_u8(K.as_ptr().add(2 * i) as *const u8)
    }
    #[target_feature(enable = "neon,aes")]
    #[inline]
    unsafe fn set(lo: u64, hi: u64) -> V {
        vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(lo), vcreate_u64(hi)))
    }
    #[target_feature(enable = "neon,aes")]
    #[inline]
    unsafe fn fold(x: V) -> u64 {
        let v = vreinterpretq_u64_u8(x);
        vgetq_lane_u64(v, 0) ^ vgetq_lane_u64(v, 1)
    }

    simd_core!(#[target_feature(enable = "neon,aes")]);

    #[cfg(all(feature = "std", not(target_feature = "aes")))]
    #[inline]
    pub(crate) fn detected() -> bool {
        std::arch::is_aarch64_feature_detected!("aes")
    }
}
