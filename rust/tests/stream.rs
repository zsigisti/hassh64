//! Hassh64Stream must equal hassh64() for every length and every way of
//! splitting the input, including digests taken mid-stream.
use hassh64::{hassh64, Hassh64Stream};

struct Rng(u64);
impl Rng {
    fn next(&mut self) -> u64 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        self.0
    }
}

fn run(m: &[u8], seed: u64, step: isize, rng: &mut Rng, check_prefixes: bool) {
    let mut st = Hassh64Stream::new(seed);
    let mut off = 0;
    while off < m.len() {
        let n = match step {
            s if s > 0 => s as usize,
            0 => (rng.next() % 300) as usize,
            _ => (rng.next() % 9000) as usize,
        }
        .min(m.len() - off);
        st.update(&m[off..off + n]);
        off += n;
        if check_prefixes {
            assert_eq!(st.digest(), hassh64(&m[..off], seed), "prefix {off} of {} step {step}", m.len());
        }
    }
    assert_eq!(st.digest(), hassh64(m, seed), "len {} seed {seed} step {step}", m.len());
    assert_eq!(st.len(), m.len() as u64);
}

#[test]
fn stream_equals_one_shot() {
    let mut rng = Rng(0x9e37_79b9_7f4a_7c15);
    let m: Vec<u8> = (0..20000).map(|_| rng.next() as u8).collect();
    let steps: [isize; 15] = [1, 3, 16, 64, 65, 127, 128, 129, 256, 4095, 4096, 4097, 8191, 0, -1];
    for len in 0..=m.len() {
        let near = len <= 400 || (3950..=4400).contains(&len) || (8150..=8300).contains(&len) || len >= 19900;
        if !near && len % 211 != 0 {
            continue;
        }
        for &step in &steps {
            if step == 1 && len > 9000 {
                continue;
            }
            run(&m[..len], (len as u64).wrapping_mul(0x100000001b3), step, &mut rng, false);
        }
    }
    for len in (4000..=4400).step_by(37) {
        run(&m[..len], 7, 0, &mut rng, true);
    }
    run(&m, 7, -1, &mut rng, true);
}

#[test]
fn stream_clone_continues() {
    let m: Vec<u8> = (0..15000u32).map(|i| (i * 31 + 7) as u8).collect();
    let mut a = Hassh64Stream::new(99);
    a.update(&m[..10000]);
    let mut b = a.clone();
    a.update(&m[10000..]);
    b.update(&m[10000..]);
    assert_eq!(a.digest(), hassh64(&m, 99));
    assert_eq!(b.digest(), a.digest());
}

#[cfg(feature = "std")]
#[test]
fn stream_io_write() {
    let m: Vec<u8> = (0..100_000u32).map(|i| (i ^ (i >> 7)) as u8).collect();
    let mut st = Hassh64Stream::new(3);
    std::io::copy(&mut &m[..], &mut st).unwrap();
    assert_eq!(st.digest(), hassh64(&m, 3));
}
