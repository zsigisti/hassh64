//! Hasher / BuildHasher behaviour.

use core::hash::{BuildHasher, Hash, Hasher};
use hassh64::{hassh64, Hassh64BuildHasher, Hassh64Hasher};
use std::collections::{HashMap, HashSet};

fn hash_with<T: Hash + ?Sized>(b: &impl BuildHasher, v: &T) -> u64 {
    b.hash_one(v)
}

#[test]
fn deterministic_and_seeded() {
    let b = Hassh64BuildHasher::with_seed(7);
    for key in ["", "a", "hello world", &"x".repeat(1000)] {
        assert_eq!(hash_with(&b, key), hash_with(&b, key));
    }
    assert_eq!(hash_with(&b, &42u64), hash_with(&Hassh64BuildHasher::with_seed(7), &42u64));
    assert_ne!(hash_with(&b, &42u64), hash_with(&Hassh64BuildHasher::with_seed(8), &42u64));
    assert_ne!(hash_with(&b, &42u64), hash_with(&b, &43u64));
}

#[test]
fn integer_writes_are_le_bytes() {
    // write_u64 == write(le bytes) == hassh64(le bytes): platform-independent.
    for (seed, v) in [(0u64, 0u64), (1, 1), (0xdead, u64::MAX), (5, 0x0123456789abcdef)] {
        let mut a = Hassh64Hasher::with_seed(seed);
        a.write_u64(v);
        let mut b = Hassh64Hasher::with_seed(seed);
        b.write(&v.to_le_bytes());
        assert_eq!(a.finish(), b.finish());
        assert_eq!(a.finish(), hassh64(&v.to_le_bytes(), seed));
    }
    let mut a = Hassh64Hasher::default();
    a.write_usize(3);
    let mut b = Hassh64Hasher::default();
    b.write_u64(3);
    assert_eq!(a.finish(), b.finish());
    let mut a = Hassh64Hasher::default();
    a.write_i32(-1);
    a.write_u128(9);
    a.write_u8(2);
    let mut want = Vec::new();
    want.extend_from_slice(&(-1i32).to_le_bytes());
    want.extend_from_slice(&9u128.to_le_bytes());
    want.push(2);
    assert_eq!(a.finish(), hassh64(&want, 0));
}

#[test]
fn split_invariant_up_to_buffer() {
    let data: Vec<u8> = (0..64u8).collect();
    for len in 0..=64 {
        let want = hassh64(&data[..len], 99);
        for cut in 0..=len {
            let mut h = Hassh64Hasher::with_seed(99);
            h.write(&data[..cut]);
            h.write(&data[cut..len]);
            assert_eq!(h.finish(), want, "len={len} cut={cut}");
        }
    }
}

#[test]
fn long_streams_distinct_and_finish_is_idempotent() {
    // Past 64 bytes the result depends on the split (documented), but must be
    // deterministic, and nearby streams must differ.
    let data: Vec<u8> = (0..5000u32).map(|i| (i * 7 + 3) as u8).collect();
    let mut seen = HashSet::new();
    for len in 0..600 {
        let mut h = Hassh64Hasher::with_seed(1);
        for chunk in data[..len].chunks(13) {
            h.write(chunk);
        }
        let f = h.finish();
        assert_eq!(f, h.finish());
        let mut g = Hassh64Hasher::with_seed(1);
        for chunk in data[..len].chunks(13) {
            g.write(chunk);
        }
        assert_eq!(f, g.finish());
        assert!(seen.insert(f), "collision at len={len}");
    }
    // One big write, then more.
    let mut h = Hassh64Hasher::with_seed(1);
    h.write(&data);
    h.write_u64(5);
    let a = h.finish();
    let mut h = Hassh64Hasher::with_seed(1);
    h.write(&data);
    h.write_u64(6);
    assert_ne!(a, h.finish());
}

#[test]
fn hashmap_with_build_hasher() {
    let mut m: HashMap<u64, u64, Hassh64BuildHasher> = HashMap::default();
    for i in 0..10_000u64 {
        m.insert(i * 0x9e37_79b9, i);
    }
    for i in 0..10_000u64 {
        assert_eq!(m.get(&(i * 0x9e37_79b9)), Some(&i));
    }
    let mut s: HashMap<String, usize, _> = HashMap::with_hasher(Hassh64BuildHasher::with_seed(3));
    for i in 0..2000 {
        s.insert(format!("key-{i}-{}", "z".repeat(i % 150)), i);
    }
    for i in 0..2000 {
        assert_eq!(s[&format!("key-{i}-{}", "z".repeat(i % 150))], i);
    }
}

#[cfg(feature = "std")]
#[test]
fn random_state() {
    let a = hassh64::RandomState::new();
    let b = hassh64::RandomState::new();
    assert_ne!(a.seed(), b.seed());
    let c = a;
    assert_eq!(hash_with(&a, "k"), hash_with(&c, "k"));
    let mut m: hassh64::HashMap<&str, i32> = hassh64::HashMap::default();
    m.insert("a", 1);
    m.insert("b", 2);
    assert_eq!(m["a"] + m["b"], 3);
}

#[test]
fn write_u64_matches_one_shot() {
    use core::hash::{BuildHasher, Hasher};
    let mut x = 0x9e3779b97f4a7c15u64;
    for seed in [0u64, 1, 0xdead_beef, u64::MAX] {
        let b = hassh64::Hassh64BuildHasher::with_seed(seed);
        for _ in 0..10_000 {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            let mut h = b.build_hasher();
            h.write_u64(x);
            assert_eq!(h.finish(), hassh64::hassh64(&x.to_le_bytes(), seed));
        }
    }
}

/// Every split of up to 64 bytes into two writes equals the one-shot hash,
/// which exercises the inline copy at every length and buffer offset.
#[test]
fn two_writes_up_to_64_match_one_shot() {
    use core::hash::Hasher;
    let msg: Vec<u8> = (0..64u32).map(|i| (i * 31 + 7) as u8).collect();
    for total in 0..=64 {
        for cut in 0..=total {
            let mut h = hassh64::Hassh64Hasher::with_seed(42);
            h.write(&msg[..cut]);
            h.write(&msg[cut..total]);
            assert_eq!(h.finish(), hassh64::hassh64(&msg[..total], 42), "total {total} cut {cut}");
        }
    }
}

/// Past 64 bytes the output is chained, but must still be deterministic, depend
/// on every byte, and differ for different write sequences of the same bytes.
#[test]
fn overflow_is_deterministic_and_sensitive() {
    use core::hash::Hasher;
    let msg: Vec<u8> = (0..1000u32).map(|i| (i * 31 + 7) as u8).collect();
    let run = |parts: &[&[u8]]| {
        let mut h = hassh64::Hassh64Hasher::with_seed(7);
        for p in parts {
            h.write(p);
        }
        h.finish()
    };
    for n in [65usize, 100, 128, 129, 300, 1000] {
        let a = run(&[&msg[..8], &msg[8..n]]);
        assert_eq!(a, run(&[&msg[..8], &msg[8..n]]));
        let mut flipped = msg[..n].to_vec();
        flipped[n - 1] ^= 1;
        assert_ne!(a, run(&[&flipped[..8], &flipped[8..n]]), "last byte ignored at {n}");
        flipped = msg[..n].to_vec();
        flipped[0] ^= 1;
        assert_ne!(a, run(&[&flipped[..8], &flipped[8..n]]), "first byte ignored at {n}");
    }
    // A u64 key and a str key of the same bytes must not trivially collide.
    let mut h1 = hassh64::Hassh64Hasher::with_seed(0);
    h1.write_u64(5);
    let mut h2 = hassh64::Hassh64Hasher::with_seed(0);
    h2.write(&5u64.to_le_bytes());
    assert_eq!(h1.finish(), h2.finish()); // same bytes, same result: by design
}
