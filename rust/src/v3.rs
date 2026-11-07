//! x86-64 VAES + AVX2 LeMac kernel for inputs of `LONG_MIN` bytes and more,
//! picked at run time in `std` builds (never in `no_std`: a kernel may not save
//! YMM state). Same algorithm and output as every other path. The step loop is
//! inline assembly, the same kernel as the C header's, so it builds on the
//! crate's MSRV (VAES intrinsics need Rust 1.89).
//!
//! State layout as in `stream.rs`: 13 slots of 32 bytes (S0..S8, R0, R1,
//! R1 ^ R2, RR), state 0 in bytes 0..15 of a slot, state 1 in 16..31.

use core::arch::asm;
use core::sync::atomic::{AtomicU8, Ordering};

use crate::scalar::seed_s;

static LEVEL: AtomicU8 = AtomicU8::new(0); // 0 unknown, 1 no, 2 yes

/// AES-NI, SSE4.1, AVX2 and VAES, with YMM state saved by the OS. Cached.
#[inline]
pub(crate) fn detected() -> bool {
    match LEVEL.load(Ordering::Relaxed) {
        2 => true,
        1 => false,
        _ => probe(),
    }
}

#[cold]
fn probe() -> bool {
    let ok = probe_cpu();
    LEVEL.store(if ok { 2 } else { 1 }, Ordering::Relaxed);
    ok
}

#[allow(unused_unsafe)]
fn probe_cpu() -> bool {
    use core::arch::x86_64::{__cpuid, __cpuid_count};
    // SAFETY: CPUID exists on every x86-64 CPU.
    let l1 = unsafe { __cpuid(1) };
    let bit = |r: u32, b: u32| r & (1 << b) != 0;
    // AES-NI, SSE4.1, OSXSAVE, AVX
    if !(bit(l1.ecx, 25) && bit(l1.ecx, 19) && bit(l1.ecx, 27) && bit(l1.ecx, 28)) {
        return false;
    }
    let (lo, _hi): (u32, u32);
    // SAFETY: OSXSAVE is set, so XGETBV is available.
    unsafe {
        asm!("xgetbv", in("ecx") 0u32, out("eax") lo, out("edx") _hi,
             options(nomem, nostack, preserves_flags));
    }
    if lo & 6 != 6 {
        return false; // the OS does not save XMM and YMM state
    }
    // SAFETY: as above.
    if unsafe { __cpuid(0) }.eax < 7 {
        return false;
    }
    let l7 = unsafe { __cpuid_count(7, 0) };
    bit(l7.ebx, 5) && bit(l7.ecx, 9) // AVX2, VAES
}

/// `n` (>= 1) LeMac steps at `q`, `q + 128`, ... on slots 0..11 of `a`; RR is
/// read from `pv + 64`. Returns the address of the last step (its bytes
/// 64..96 are the new RR).
///
/// # Safety
/// [`detected`] must be true; `q` must point to `128 * n` readable bytes and
/// `pv` to 96.
#[target_feature(enable = "avx")]
unsafe fn steps(a: *mut u8, mut q: *const u8, mut n: usize, mut pv: *const u8) -> *const u8 {
    asm!(
        "vmovdqu 0x000({a}), %ymm0", "vmovdqu 0x020({a}), %ymm1",
        "vmovdqu 0x040({a}), %ymm2", "vmovdqu 0x060({a}), %ymm3",
        "vmovdqu 0x080({a}), %ymm4", "vmovdqu 0x0a0({a}), %ymm5",
        "vmovdqu 0x0c0({a}), %ymm6", "vmovdqu 0x0e0({a}), %ymm7",
        "vmovdqu 0x100({a}), %ymm8", "vmovdqu 0x120({a}), %ymm9",
        "vmovdqu 0x140({a}), %ymm10", "vmovdqu 0x160({a}), %ymm11",
        ".p2align 5",
        "2:",
        "prefetcht0 1024({q})",
        "prefetcht0 1088({q})",
        "vmovdqu 64({q}), %ymm14",   // M2
        "vmovdqu 96({q}), %ymm15",   // M3
        "vmovdqu 32({q}), %ymm13",   // M1
        "vmovdqu ({q}), %ymm12",     // M0
        "vpxor %ymm8, %ymm14, %ymm14",
        "vpxor %ymm0, %ymm14, %ymm14", // new S0 = S0 ^ S8 ^ M2
        "vaesenc %ymm15, %ymm7, %ymm8",
        "vaesenc %ymm13, %ymm6, %ymm7",
        "vaesenc %ymm13, %ymm5, %ymm6",
        "vaesenc %ymm12, %ymm4, %ymm5",
        "vaesenc %ymm12, %ymm3, %ymm4",
        "vaesenc %ymm11, %ymm2, %ymm3",
        "vaesenc %ymm15, %ymm1, %ymm2",
        "vaesenc %ymm15, %ymm0, %ymm1",
        "vmovdqa %ymm14, %ymm0",
        "vpxor %ymm9, %ymm10, %ymm11",  // R1 ^ R2 for the next step
        "vmovdqa %ymm9, %ymm10",        // R1 = R0
        "vpxor 64({pv}), %ymm13, %ymm9", // R0 = RR ^ M1
        "mov {q}, {pv}",
        "add $128, {q}",
        "dec {n}",
        "jnz 2b",
        "vmovdqu %ymm0, 0x000({a})", "vmovdqu %ymm1, 0x020({a})",
        "vmovdqu %ymm2, 0x040({a})", "vmovdqu %ymm3, 0x060({a})",
        "vmovdqu %ymm4, 0x080({a})", "vmovdqu %ymm5, 0x0a0({a})",
        "vmovdqu %ymm6, 0x0c0({a})", "vmovdqu %ymm7, 0x0e0({a})",
        "vmovdqu %ymm8, 0x100({a})", "vmovdqu %ymm9, 0x120({a})",
        "vmovdqu %ymm10, 0x140({a})", "vmovdqu %ymm11, 0x160({a})",
        "vzeroupper",
        a = in(reg) a,
        q = inout(reg) q,
        n = inout(reg) n,
        pv = inout(reg) pv,
        out("ymm0") _, out("ymm1") _, out("ymm2") _, out("ymm3") _,
        out("ymm4") _, out("ymm5") _, out("ymm6") _, out("ymm7") _,
        out("ymm8") _, out("ymm9") _, out("ymm10") _, out("ymm11") _,
        out("ymm12") _, out("ymm13") _, out("ymm14") _, out("ymm15") _,
        options(att_syntax, nostack),
    );
    let _ = (q, n);
    pv
}

/// `n` (>= 1) LeMac steps from `q` (at least `128 * n` bytes) on the state `a`.
///
/// # Safety
/// [`detected`] must be true.
pub(crate) unsafe fn absorb(a: &mut [u8; 416], q: &[u8], n: usize) {
    assert!(n >= 1 && q.len() >= 128 * n);
    let mut pb = [0u8; 96];
    pb[64..96].copy_from_slice(&a[384..416]);
    let last = steps(a.as_mut_ptr(), q.as_ptr(), n, pb.as_ptr());
    // `last` is the final step's address inside `q`.
    let off = last as usize - q.as_ptr() as usize;
    a[384..416].copy_from_slice(&q[off + 64..off + 96]);
}

/// hassh64 of `data` (`data.len() >= LONG_MIN`).
///
/// # Safety
/// [`detected`] must be true.
pub(crate) unsafe fn long(data: &[u8], seed: u64) -> u64 {
    let len = data.len();
    let s = seed_s(seed);
    let mut a = [0u8; 416];
    // AES-NI and SSE4.1 are implied by `detected`.
    crate::simd::x86::lm_begin(&mut a, s);
    let nsteps = (len + 127) / 128;
    if nsteps > 1 {
        absorb(&mut a, &data[..128 * (nsteps - 1)], nsteps - 1);
    }
    absorb(&mut a, &data[len - 128..], 1); // the final step, aligned to the end
    crate::simd::x86::lm_finish(&a, s, len as u64)
}
