//! Known-answer tests: all 548 frozen vectors from test/vectors.h (copied into
//! this crate so the published package is self-contained), on every path.

use core::hash::Hasher;
use hassh64::__private::{aesenc_portable, hassh64_accel, hassh64_portable, path_name};
use hassh64::{hassh64, Hassh64Hasher};

const VECTORS_H: &str = include_str!("vectors.h");

fn vectors() -> Vec<(usize, u64, u64)> {
    let mut v = Vec::new();
    for line in VECTORS_H.lines() {
        let line = line.trim();
        if !line.starts_with('{') || !line.ends_with("},") {
            continue;
        }
        let body = &line[1..line.len() - 2];
        let f: Vec<&str> = body.split(',').map(str::trim).collect();
        if f.len() != 3 {
            continue;
        }
        let hex = |s: &str| {
            let s = s.strip_prefix("0x").unwrap().strip_suffix("ull").unwrap();
            u64::from_str_radix(s, 16).unwrap()
        };
        v.push((f[0].parse().unwrap(), hex(f[1]), hex(f[2])));
    }
    v
}

fn message(n: usize) -> Vec<u8> {
    (0..n).map(|i| ((i * 31 + 7) % 256) as u8).collect()
}

fn check(name: &str, f: impl Fn(&[u8], u64) -> u64) -> usize {
    let vecs = vectors();
    assert_eq!(vecs.len(), 548, "vector file parse");
    let msg = message(vecs.iter().map(|v| v.0).max().unwrap());
    let mut bad = Vec::new();
    for &(len, seed, want) in &vecs {
        // Hash a copy at an odd offset too, so the unaligned loads are exercised.
        let mut shifted = vec![0u8; len + 1];
        shifted[1..].copy_from_slice(&msg[..len]);
        for data in [&msg[..len], &shifted[1..]] {
            let got = f(data, seed);
            if got != want {
                bad.push(format!("len={len} seed={seed:016x} want={want:016x} got={got:016x}"));
            }
        }
    }
    assert!(bad.is_empty(), "{name}: {} failures, first: {:?}", bad.len(), &bad[..bad.len().min(10)]);
    eprintln!("{name}: {} vectors ok (hassh64 default path here: {})", vecs.len(), path_name());
    vecs.len()
}

#[test]
fn vectors_default_path() {
    check("hassh64", hassh64);
}

#[test]
fn vectors_portable_path() {
    check("portable", hassh64_portable);
}

#[test]
fn vectors_accel_path() {
    if hassh64_accel(&[0u8; 65], 0).is_none() {
        eprintln!("no hardware AES path in this build/CPU; skipped");
        return;
    }
    check("accel", |d, s| hassh64_accel(d, s).unwrap());
}

#[test]
fn vectors_hasher_single_write() {
    // A single write of <= 64 bytes must equal hassh64.
    let vecs = vectors();
    let msg = message(64);
    let mut n = 0;
    for &(len, seed, want) in vecs.iter().filter(|v| v.0 <= 64) {
        let mut h = Hassh64Hasher::with_seed(seed);
        h.write(&msg[..len]);
        assert_eq!(h.finish(), want, "len={len}");
        n += 1;
    }
    assert!(n > 100);
}

#[test]
fn aesenc_intel_example() {
    // Intel's AES-NI white paper writes these as 128-bit integers, most
    // significant byte first, so byte 0 of the block is the LAST hex pair.
    // (SPEC.md says "bytes 0..15", which is the reverse order.)
    let hex = |s: &str| {
        let mut b = [0u8; 16];
        for i in 0..16 {
            b[15 - i] = u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap();
        }
        b
    };
    let x = hex("7b5b54657374566563746f725d53475d");
    let k = hex("48692853686179295b477565726f6e5d");
    assert_eq!(aesenc_portable(&x, &k), hex("a8311c2f9fdba3c58b104b58ded7e595"));
}

#[test]
fn readme_examples() {
    assert_eq!(hassh64(b"hello", 0), 0xedfe40b18d046dba);
    assert_eq!(hassh64(b"hello", 42), 0x825305b7d83ed2a7);
}

#[test]
fn paths_agree_on_random_inputs() {
    // xorshift-generated lengths 0..2100 and seeds; portable == default == accel.
    let mut x = 0x9e3779b97f4a7c15u64;
    let mut next = || {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        x
    };
    let buf: Vec<u8> = (0..2200).map(|_| next() as u8).collect();
    for len in 0..2100 {
        let seed = next();
        let off = (next() % 64) as usize;
        let d = &buf[off..off + len];
        let a = hassh64(d, seed);
        assert_eq!(a, hassh64_portable(d, seed), "len={len}");
        if let Some(b) = hassh64_accel(d, seed) {
            assert_eq!(a, b, "len={len}");
        }
    }
}
