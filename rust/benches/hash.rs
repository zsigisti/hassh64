//! Latency-style benchmarks: each call's output is XORed into the first 8 bytes
//! of the next call's key (store, then hash: the hash-table case), so calls form
//! a dependency chain and the time per call is latency, not throughput.
//! Also a u64-key HashMap insert + lookup microbench.
//!
//!     cargo bench --bench hash
//!     RUSTFLAGS="-C target-cpu=native" cargo bench --bench hash

use core::hash::BuildHasher;
use criterion::{criterion_group, criterion_main, BenchmarkId, Criterion, Throughput};
use std::collections::HashMap;
use std::hint::black_box;
use std::time::{Duration, Instant};

const SIZES: [usize; 7] = [8, 16, 32, 64, 128, 256, 1024];
const SEED: u64 = 0x0123_4567_89ab_cdef;

fn chain(iters: u64, len: usize, f: &dyn Fn(&[u8]) -> u64) -> Duration {
    let mut buf: Vec<u8> = (0..len.max(8)).map(|i| (i * 31 + 7) as u8).collect();
    let mut h = 0u64;
    let t = Instant::now();
    for _ in 0..iters {
        let first = u64::from_le_bytes(buf[..8].try_into().unwrap()) ^ h;
        buf[..8].copy_from_slice(&first.to_le_bytes());
        h = f(black_box(&buf[..len]));
    }
    black_box(h);
    t.elapsed()
}

fn bytes(c: &mut Criterion) {
    let foldhash_s = foldhash::fast::FixedState::with_seed(SEED);
    let ahash_s = ahash::RandomState::with_seeds(1, 2, 3, 4);
    let rapid_secrets = rapidhash::v3::RapidSecrets::seed(SEED);
    let hassh_s = hassh64::Hassh64BuildHasher::with_seed(SEED);

    let mut g = c.benchmark_group("latency");
    g.warm_up_time(Duration::from_millis(500)).measurement_time(Duration::from_secs(2));
    for &len in &SIZES {
        g.throughput(Throughput::Bytes(len as u64));
        let mut run = |name: &str, f: &dyn Fn(&[u8]) -> u64| {
            g.bench_with_input(BenchmarkId::new(name, len), &len, |b, &len| {
                b.iter_custom(|n| chain(n, len, f))
            });
        };
        run("hassh64", &|d| hassh64::hassh64(d, SEED));
        run("hassh64-hasher", &|d| hassh_s.hash_one(d));
        run("rapidhash-v3", &|d| rapidhash::v3::rapidhash_v3_seeded(d, &rapid_secrets));
        run("foldhash-fast", &|d| foldhash_s.hash_one(d));
        run("ahash", &|d| ahash_s.hash_one(d));
    }
    g.finish();
}

fn map_u64<S: BuildHasher + Clone>(c: &mut Criterion, name: &str, s: S) {
    const N: u64 = 4096;
    let keys: Vec<u64> = (0..N).map(|i| i.wrapping_mul(0x9e37_79b9_7f4a_7c15)).collect();
    c.bench_function(&format!("hashmap-u64-insert+lookup/{name}"), |b| {
        b.iter(|| {
            let mut m: HashMap<u64, u64, S> = HashMap::with_capacity_and_hasher(N as usize, s.clone());
            for &k in &keys {
                m.insert(k, k);
            }
            let mut sum = 0u64;
            for &k in &keys {
                sum = sum.wrapping_add(*m.get(&k).unwrap());
            }
            black_box(sum)
        })
    });
}

fn maps(c: &mut Criterion) {
    map_u64(c, "hassh64", hassh64::Hassh64BuildHasher::with_seed(SEED));
    map_u64(c, "rapidhash-fast", rapidhash::fast::RandomState::new());
    map_u64(c, "foldhash-fast", foldhash::fast::FixedState::with_seed(SEED));
    map_u64(c, "ahash", ahash::RandomState::with_seeds(1, 2, 3, 4));
    map_u64(c, "std-siphash", std::collections::hash_map::RandomState::new());
}

criterion_group!(benches, bytes, maps);
criterion_main!(benches);
