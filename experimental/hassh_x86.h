/*
 * hassh64 x86-64 experimental kernels: run-time selection of x86-64-v2
 * (AES-NI), v3 (VAES + AVX2) and v4 (VAES + AVX-512) code paths, with the hot
 * loops in inline assembly. Output is identical to hassh64() in ../hassh.h on
 * every path; test.c checks that, and `make test-sde` runs the v4 code under
 * Intel SDE on CPUs without AVX-512.
 *
 * EXPERIMENTAL: not part of the stable single header. GCC or Clang, x86-64.
 */
#ifndef HASSH_X86_H
#define HASSH_X86_H
#define HASSH_WANT_PORTABLE
#ifndef HASSH_HEADER_PATH
#  define HASSH_HEADER_PATH "../hassh.h"
#endif
#include HASSH_HEADER_PATH
#include <immintrin.h>
#include <cpuid.h>

#if !defined(__x86_64__) || !(defined(__GNUC__) || defined(__clang__))
#  error "hassh_x86.h: x86-64 with GCC or Clang only"
#endif

/* ------------------------------------------------------------ CPU levels */
enum { HX_PORTABLE = 0, HX_V2 = 2, HX_V3 = 3, HX_V4 = 4 };

static inline unsigned long long hx_xgetbv0(void) {
    unsigned lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((unsigned long long)hi << 32) | lo;
}
/* Highest level whose instructions the CPU has and the OS saves state for. */
static inline int hx_detect(void) {
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return HX_PORTABLE;
    int aes = (c >> 25) & 1, sse41 = (c >> 19) & 1, osxsave = (c >> 27) & 1, avx = (c >> 28) & 1;
    if (!(aes && sse41)) return HX_PORTABLE;
    if (!(osxsave && avx)) return HX_V2;
    unsigned long long xcr0 = hx_xgetbv0();
    if ((xcr0 & 0x6) != 0x6) return HX_V2;                       /* XMM + YMM state */
    if (__get_cpuid_max(0, 0) < 7) return HX_V2;
    __cpuid_count(7, 0, a, b, c, d);
    int avx2 = (b >> 5) & 1, vaes = (c >> 9) & 1;
    int f = (b >> 16) & 1, bw = (b >> 30) & 1, vl = (b >> 31) & 1;
    if (!(avx2 && vaes)) return HX_V2;
    if (f && bw && vl && (xcr0 & 0xe6) == 0xe6) return HX_V4;    /* + opmask, ZMM_Hi256, Hi16_ZMM */
    return HX_V3;
}
static int hx_level_cache = -1;
static inline int hx_level(void) {
    int l = __atomic_load_n(&hx_level_cache, __ATOMIC_RELAXED);
    if (l < 0) { l = hx_detect(); __atomic_store_n(&hx_level_cache, l, __ATOMIC_RELAXED); }
    return l;
}

/* --------------------------------------------------------- shared pieces */
#define HX_K(i)  (*(const __m128i *)(const void *)&HASSH_K[2 * (i)])
#define HX_L(w)  (*(const __m128i *)(const void *)&HASSH_L[2 * (w)])
static const uint8_t hx_zero128[128] __attribute__((aligned(64))) = {0};

__attribute__((target("aes,sse4.1")))
static inline __m128i hx_sv(uint64_t s) {
    return _mm_set_epi64x((long long)hassh_fold(s ^ HASSH_P[2], HASSH_P[3]), (long long)s);
}
__attribute__((target("aes,sse4.1")))
static inline __m128i hx_lv(uint64_t s, size_t len) {
    uint64_t lo, hi; hassh_lenvec_s(s, len, &lo, &hi);
    return _mm_aesenc_si128(_mm_aesenc_si128(_mm_set_epi64x((long long)hi, (long long)lo), HX_K(30)), HX_K(31));
}
__attribute__((target("aes,sse4.1")))
static inline uint64_t hx_out(__m128i x) {
    x = _mm_aesenc_si128(x, HX_K(24)); x = _mm_aesenc_si128(x, HX_K(25)); x = _mm_aesenc_si128(x, HX_K(26));
    return (uint64_t)_mm_cvtsi128_si64(x) ^ (uint64_t)_mm_extract_epi64(x, 1);
}
__attribute__((target("aes,sse4.1")))
static inline __m128i hx_init(int i, __m128i k, __m128i sv) {
    (void)i;
    return _mm_aesenc_si128(_mm_aesenc_si128(_mm_xor_si128(k, sv), HX_K(30)), HX_K(31));
}

/* ============================================================ v3 (VAES) */
#define HX3 __attribute__((target("aes,sse4.1,avx2,vaes")))
HX3 static inline __m256i hx3_ld(const uint8_t *p) { return _mm256_loadu_si256((const __m256i *)(const void *)p); }
HX3 static inline __m256i hx3_bk(int i) { return _mm256_broadcastsi128_si256(HX_K(i)); }
HX3 static inline __m256i hx3_perm(__m256i x) {
    x = _mm256_aesenc_epi128(x, hx3_bk(16)); x = _mm256_aesenc_epi128(x, hx3_bk(17));
    return _mm256_aesenc_epi128(x, hx3_bk(18));
}
/* lane starts j (lanes 2j, 2j+1) */
HX3 static inline __m256i hx3_start(int j, __m256i sv2, const uint8_t *st) {
    if (st) return hx3_ld(st + 32 * j);
    __m256i x = _mm256_xor_si256(hx3_ld((const uint8_t *)&HASSH_K[4 * j]), sv2);
    return _mm256_aesenc_epi128(_mm256_aesenc_epi128(x, hx3_bk(30)), hx3_bk(31));
}
HX3 static inline uint64_t hx3_fold(__m256i x, uint64_t s, size_t len) {
    return hx_out(_mm_xor_si128(_mm_xor_si128(_mm256_castsi256_si128(x), _mm256_extracti128_si256(x, 1)), hx_lv(s, len)));
}
/* 65..4095 bytes */
HX3 static uint64_t hx3_lanes(const uint8_t *p, size_t len, uint64_t s, const uint8_t *st) {
    const __m256i sv2 = _mm256_broadcastsi128_si256(hx_sv(s));
    if (len <= 128) {
        const uint8_t *e = p + len;
        __m256i a = hx3_perm(_mm256_xor_si256(hx3_start(0, sv2, st), hx3_ld(p)));
        __m256i b = hx3_perm(_mm256_xor_si256(hx3_start(1, sv2, st), hx3_ld(p + 32)));
        __m256i c = hx3_perm(_mm256_xor_si256(hx3_start(2, sv2, st), hx3_ld(e - 64)));
        __m256i d = hx3_perm(_mm256_xor_si256(hx3_start(3, sv2, st), hx3_ld(e - 32)));
        return hx3_fold(_mm256_xor_si256(_mm256_xor_si256(a, b), _mm256_xor_si256(c, d)), s, len);
    }
    __m256i a0 = hx3_start(0, sv2, st), a1 = hx3_start(1, sv2, st), a2 = hx3_start(2, sv2, st), a3 = hx3_start(3, sv2, st);
    __m256i a4 = hx3_start(4, sv2, st), a5 = hx3_start(5, sv2, st), a6 = hx3_start(6, sv2, st), a7 = hx3_start(7, sv2, st);
#define HX3_STRIPE(q, r)                                                                   \
    a0 = hx3_perm(_mm256_xor_si256(a0, hx3_ld((q) + 0))); a1 = hx3_perm(_mm256_xor_si256(a1, hx3_ld((q) + 32))); \
    a2 = hx3_perm(_mm256_xor_si256(a2, hx3_ld((q) + 64))); a3 = hx3_perm(_mm256_xor_si256(a3, hx3_ld((q) + 96))); \
    a4 = hx3_perm(_mm256_xor_si256(a4, hx3_ld((r) + 0))); a5 = hx3_perm(_mm256_xor_si256(a5, hx3_ld((r) + 32))); \
    a6 = hx3_perm(_mm256_xor_si256(a6, hx3_ld((r) + 64))); a7 = hx3_perm(_mm256_xor_si256(a7, hx3_ld((r) + 96)));
    if (len <= 256) {
        HX3_STRIPE(p, p + len - 128)
    } else {
        const uint8_t *last = p + len - 256;
        for (;;) {
            const uint8_t *q = p < last ? p : last;
            HX3_STRIPE(q, q + 128)
            if (q == last) break;
            p += 256;
        }
    }
#undef HX3_STRIPE
    __m256i x = _mm256_xor_si256(_mm256_xor_si256(_mm256_xor_si256(a0, a1), _mm256_xor_si256(a2, a3)),
                                 _mm256_xor_si256(_mm256_xor_si256(a4, a5), _mm256_xor_si256(a6, a7)));
    return hx3_fold(x, s, len);
}

/* LeMac, n steps (n >= 1) at q, q+128, ...: A = S0..S8, R0, R1, R1^R2 (ymm, both
 * states); RR re-read from *prev + 64. Same kernel as in hassh.h. */
static inline void hx3_lm_steps(__m256i *A, const uint8_t *q, size_t n, const uint8_t **prev) {
    const uint8_t *pv = *prev;
    __asm__ volatile(
        "vmovdqu 0x000(%[a]), %%ymm0\n\t"  "vmovdqu 0x020(%[a]), %%ymm1\n\t"
        "vmovdqu 0x040(%[a]), %%ymm2\n\t"  "vmovdqu 0x060(%[a]), %%ymm3\n\t"
        "vmovdqu 0x080(%[a]), %%ymm4\n\t"  "vmovdqu 0x0a0(%[a]), %%ymm5\n\t"
        "vmovdqu 0x0c0(%[a]), %%ymm6\n\t"  "vmovdqu 0x0e0(%[a]), %%ymm7\n\t"
        "vmovdqu 0x100(%[a]), %%ymm8\n\t"  "vmovdqu 0x120(%[a]), %%ymm9\n\t"
        "vmovdqu 0x140(%[a]), %%ymm10\n\t" "vmovdqu 0x160(%[a]), %%ymm11\n\t"
        ".p2align 5\n"
        "1:\n\t"
        "prefetcht0 1024(%[q])\n\t" "prefetcht0 1088(%[q])\n\t"
        "vmovdqu 64(%[q]), %%ymm14\n\t" "vmovdqu 96(%[q]), %%ymm15\n\t"
        "vmovdqu 32(%[q]), %%ymm13\n\t" "vmovdqu (%[q]), %%ymm12\n\t"
        "vpxor %%ymm8, %%ymm14, %%ymm14\n\t" "vpxor %%ymm0, %%ymm14, %%ymm14\n\t"
        "vaesenc %%ymm15, %%ymm7, %%ymm8\n\t"  "vaesenc %%ymm13, %%ymm6, %%ymm7\n\t"
        "vaesenc %%ymm13, %%ymm5, %%ymm6\n\t"  "vaesenc %%ymm12, %%ymm4, %%ymm5\n\t"
        "vaesenc %%ymm12, %%ymm3, %%ymm4\n\t"  "vaesenc %%ymm11, %%ymm2, %%ymm3\n\t"
        "vaesenc %%ymm15, %%ymm1, %%ymm2\n\t"  "vaesenc %%ymm15, %%ymm0, %%ymm1\n\t"
        "vmovdqa %%ymm14, %%ymm0\n\t"
        "vpxor %%ymm9, %%ymm10, %%ymm11\n\t" "vmovdqa %%ymm9, %%ymm10\n\t"
        "vpxor 64(%[pv]), %%ymm13, %%ymm9\n\t"
        "mov %[q], %[pv]\n\t" "add $128, %[q]\n\t" "dec %[n]\n\t" "jnz 1b\n\t"
        "vmovdqu %%ymm0, 0x000(%[a])\n\t"  "vmovdqu %%ymm1, 0x020(%[a])\n\t"
        "vmovdqu %%ymm2, 0x040(%[a])\n\t"  "vmovdqu %%ymm3, 0x060(%[a])\n\t"
        "vmovdqu %%ymm4, 0x080(%[a])\n\t"  "vmovdqu %%ymm5, 0x0a0(%[a])\n\t"
        "vmovdqu %%ymm6, 0x0c0(%[a])\n\t"  "vmovdqu %%ymm7, 0x0e0(%[a])\n\t"
        "vmovdqu %%ymm8, 0x100(%[a])\n\t"  "vmovdqu %%ymm9, 0x120(%[a])\n\t"
        "vmovdqu %%ymm10, 0x140(%[a])\n\t" "vmovdqu %%ymm11, 0x160(%[a])\n\t"
        : [q] "+r"(q), [n] "+r"(n), [pv] "+r"(pv) : [a] "r"(A)
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
    *prev = pv;
}
/* LeMac with AVX-512VL: 32 registers keep RR (no re-read) and a spare copy;
 * vpternlogq does the 3-way XOR. A = S0..S8, RR, R0, R1, R1^R2. */
static inline void hx4_lm_steps(__m256i *A, const uint8_t *q, size_t n) {
    __asm__ volatile(
        "vmovdqu64 0x000(%[a]), %%ymm0\n\t"  "vmovdqu64 0x020(%[a]), %%ymm1\n\t"
        "vmovdqu64 0x040(%[a]), %%ymm2\n\t"  "vmovdqu64 0x060(%[a]), %%ymm3\n\t"
        "vmovdqu64 0x080(%[a]), %%ymm4\n\t"  "vmovdqu64 0x0a0(%[a]), %%ymm5\n\t"
        "vmovdqu64 0x0c0(%[a]), %%ymm6\n\t"  "vmovdqu64 0x0e0(%[a]), %%ymm7\n\t"
        "vmovdqu64 0x100(%[a]), %%ymm8\n\t"  "vmovdqu64 0x120(%[a]), %%ymm9\n\t"
        "vmovdqu64 0x140(%[a]), %%ymm10\n\t" "vmovdqu64 0x160(%[a]), %%ymm11\n\t"
        "vmovdqu64 0x180(%[a]), %%ymm12\n\t"
        ".p2align 5\n"
        "1:\n\t"
        "prefetcht0 1024(%[q])\n\t" "prefetcht0 1088(%[q])\n\t"
        "vmovdqu64 (%[q]), %%ymm16\n\t"   "vmovdqu64 32(%[q]), %%ymm17\n\t"
        "vmovdqu64 64(%[q]), %%ymm18\n\t" "vmovdqu64 96(%[q]), %%ymm19\n\t"
        "vmovdqa64 %%ymm0, %%ymm20\n\t"
        "vpternlogq $0x96, %%ymm18, %%ymm8, %%ymm20\n\t"     /* new S0 = S0 ^ S8 ^ M2 */
        "vaesenc %%ymm19, %%ymm7, %%ymm8\n\t"  "vaesenc %%ymm17, %%ymm6, %%ymm7\n\t"
        "vaesenc %%ymm17, %%ymm5, %%ymm6\n\t"  "vaesenc %%ymm16, %%ymm4, %%ymm5\n\t"
        "vaesenc %%ymm16, %%ymm3, %%ymm4\n\t"  "vaesenc %%ymm12, %%ymm2, %%ymm3\n\t"
        "vaesenc %%ymm19, %%ymm1, %%ymm2\n\t"  "vaesenc %%ymm19, %%ymm0, %%ymm1\n\t"
        "vmovdqa64 %%ymm20, %%ymm0\n\t"
        "vpxorq %%ymm10, %%ymm11, %%ymm12\n\t"               /* R1^R2 for the next step */
        "vmovdqa64 %%ymm10, %%ymm11\n\t"                     /* R1 = R0 */
        "vpxorq %%ymm17, %%ymm9, %%ymm10\n\t"                /* R0 = RR ^ M1 */
        "vmovdqa64 %%ymm18, %%ymm9\n\t"                      /* RR = M2 */
        "add $128, %[q]\n\t" "dec %[n]\n\t" "jnz 1b\n\t"
        "vmovdqu64 %%ymm0, 0x000(%[a])\n\t"  "vmovdqu64 %%ymm1, 0x020(%[a])\n\t"
        "vmovdqu64 %%ymm2, 0x040(%[a])\n\t"  "vmovdqu64 %%ymm3, 0x060(%[a])\n\t"
        "vmovdqu64 %%ymm4, 0x080(%[a])\n\t"  "vmovdqu64 %%ymm5, 0x0a0(%[a])\n\t"
        "vmovdqu64 %%ymm6, 0x0c0(%[a])\n\t"  "vmovdqu64 %%ymm7, 0x0e0(%[a])\n\t"
        "vmovdqu64 %%ymm8, 0x100(%[a])\n\t"  "vmovdqu64 %%ymm9, 0x120(%[a])\n\t"
        "vmovdqu64 %%ymm10, 0x140(%[a])\n\t" "vmovdqu64 %%ymm11, 0x160(%[a])\n\t"
        "vmovdqu64 %%ymm12, 0x180(%[a])\n\t"
        : [q] "+r"(q), [n] "+r"(n) : [a] "r"(A)
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm16", "xmm17", "xmm18", "xmm19", "xmm20");
}

/* LeMac finish (both levels): 4 blank steps, compression, lv, fin. A = S0..S8,
 * RR, R0, R1, R1^R2. */
HX3 static uint64_t hx3_lm_finish(__m256i *A, uint64_t s, size_t len) {
    __m256i s0 = A[0], s1 = A[1], s2 = A[2], s3 = A[3], s4 = A[4], s5 = A[5], s6 = A[6], s7 = A[7], s8 = A[8];
    __m256i rr = A[9], r0 = A[10], r1 = A[11], x12 = A[12];
    for (int b = 0; b < 4; b++) {               /* message words all zero */
        __m256i t = _mm256_xor_si256(s0, s8);
        s8 = _mm256_aesenc_epi128(s7, _mm256_setzero_si256()); s7 = _mm256_aesenc_epi128(s6, _mm256_setzero_si256());
        s6 = _mm256_aesenc_epi128(s5, _mm256_setzero_si256()); s5 = _mm256_aesenc_epi128(s4, _mm256_setzero_si256());
        s4 = _mm256_aesenc_epi128(s3, _mm256_setzero_si256()); s3 = _mm256_aesenc_epi128(s2, x12);
        s2 = _mm256_aesenc_epi128(s1, _mm256_setzero_si256()); s1 = _mm256_aesenc_epi128(s0, _mm256_setzero_si256());
        s0 = t;
        x12 = _mm256_xor_si256(r0, r1); r1 = r0; r0 = rr; rr = _mm256_setzero_si256();
    }
    const __m256i w[9] = { s0, s1, s2, s3, s4, s5, s6, s7, s8 };
    __m256i acc = _mm256_setzero_si256();
    _Pragma("GCC unroll 16") for (int i = 0; i < 9; i++) {
        __m256i y = _mm256_xor_si256(w[i], _mm256_set_m128i(HX_L(27 + i), HX_L(18 + i)));
        y = _mm256_aesenc_epi128(y, hx3_bk(16)); y = _mm256_aesenc_epi128(y, hx3_bk(17));
        y = _mm256_aesenc_epi128(y, hx3_bk(18)); y = _mm256_aesenc_epi128(y, hx3_bk(19));
        acc = _mm256_xor_si256(acc, y);
    }
    return hx3_fold(acc, s, len);
}
HX3 static void hx3_lm_start(__m256i *A, uint64_t s, const uint8_t *ls) {
    if (ls) {
        for (int i = 0; i < 9; i++) A[i] = hx3_ld(ls + 32 * i);
    } else {
        const __m256i sv2 = _mm256_broadcastsi128_si256(hx_sv(s));
        for (int i = 0; i < 9; i++) {
            __m256i x = _mm256_xor_si256(_mm256_set_m128i(HX_L(9 + i), HX_L(i)), sv2);
            A[i] = _mm256_aesenc_epi128(_mm256_aesenc_epi128(x, hx3_bk(30)), hx3_bk(31));
        }
    }
    for (int i = 9; i < 13; i++) A[i] = _mm256_setzero_si256();
}
/* 4096+ bytes */
HX3 static uint64_t hx3_long(const uint8_t *p, size_t len, uint64_t s, const uint8_t *ls) {
    __m256i A[13];                  /* S0..S8, R0, R1, R1^R2 (RR comes from memory) */
    hx3_lm_start(A, s, ls);
    const uint8_t *prev = hx_zero128;
    size_t nsteps = (len + 127) / 128;
    if (nsteps > 1) hx3_lm_steps(A, p, nsteps - 1, &prev);
    hx3_lm_steps(A, p + len - 128, 1, &prev);
    __m256i F[13];
    for (int i = 0; i < 9; i++) F[i] = A[i];
    F[9] = hx3_ld(prev + 64); F[10] = A[9]; F[11] = A[10]; F[12] = A[11];
    return hx3_lm_finish(F, s, len);
}

/* ======================================================= v4 (AVX-512 + VAES) */
#define HX4 __attribute__((target("aes,sse4.1,avx2,vaes,avx512f,avx512vl,avx512bw")))
HX4 static inline __m512i hx4_ld(const uint8_t *p) { return _mm512_loadu_si512((const void *)p); }
HX4 static inline __m512i hx4_bk(int i) { return _mm512_broadcast_i32x4(HX_K(i)); }
HX4 static inline __m512i hx4_perm(__m512i x) {
    x = _mm512_aesenc_epi128(x, hx4_bk(16)); x = _mm512_aesenc_epi128(x, hx4_bk(17));
    return _mm512_aesenc_epi128(x, hx4_bk(18));
}
HX4 static inline __m512i hx4_start(int j, __m512i sv4, const uint8_t *st) {   /* lanes 4j..4j+3 */
    if (st) return hx4_ld(st + 64 * j);
    __m512i x = _mm512_xor_si512(hx4_ld((const uint8_t *)&HASSH_K[8 * j]), sv4);
    return _mm512_aesenc_epi128(_mm512_aesenc_epi128(x, hx4_bk(30)), hx4_bk(31));
}
HX4 static inline uint64_t hx4_fold(__m512i x, uint64_t s, size_t len) {
    __m256i y = _mm256_xor_si256(_mm512_castsi512_si256(x), _mm512_extracti64x4_epi64(x, 1));
    return hx_out(_mm_xor_si128(_mm_xor_si128(_mm256_castsi256_si128(y), _mm256_extracti128_si256(y, 1)), hx_lv(s, len)));
}
/* 65..4095 bytes: 16 lanes in 4 zmm (8 in 2 for <= 128 bytes). */
HX4 static uint64_t hx4_lanes(const uint8_t *p, size_t len, uint64_t s, const uint8_t *st) {
    const __m512i sv4 = _mm512_broadcast_i32x4(hx_sv(s));
    if (len <= 128) {
        __m512i a = hx4_perm(_mm512_xor_si512(hx4_start(0, sv4, st), hx4_ld(p)));
        __m512i b = hx4_perm(_mm512_xor_si512(hx4_start(1, sv4, st), hx4_ld(p + len - 64)));
        return hx4_fold(_mm512_xor_si512(a, b), s, len);
    }
    __m512i a0 = hx4_start(0, sv4, st), a1 = hx4_start(1, sv4, st), a2 = hx4_start(2, sv4, st), a3 = hx4_start(3, sv4, st);
    if (len <= 256) {
        const uint8_t *r = p + len - 128;
        a0 = hx4_perm(_mm512_xor_si512(a0, hx4_ld(p)));      a1 = hx4_perm(_mm512_xor_si512(a1, hx4_ld(p + 64)));
        a2 = hx4_perm(_mm512_xor_si512(a2, hx4_ld(r)));      a3 = hx4_perm(_mm512_xor_si512(a3, hx4_ld(r + 64)));
    } else {
        const uint8_t *last = p + len - 256;
        for (;;) {
            const uint8_t *q = p < last ? p : last;
            a0 = hx4_perm(_mm512_xor_si512(a0, hx4_ld(q)));       a1 = hx4_perm(_mm512_xor_si512(a1, hx4_ld(q + 64)));
            a2 = hx4_perm(_mm512_xor_si512(a2, hx4_ld(q + 128))); a3 = hx4_perm(_mm512_xor_si512(a3, hx4_ld(q + 192)));
            if (q == last) break;
            p += 256;
        }
    }
    return hx4_fold(_mm512_ternarylogic_epi64(a0, a1, _mm512_xor_si512(a2, a3), 0x96), s, len);
}
HX4 static uint64_t hx4_long(const uint8_t *p, size_t len, uint64_t s, const uint8_t *ls) {
    __m256i A[13];                  /* S0..S8, RR, R0, R1, R1^R2 */
    hx3_lm_start(A, s, ls);
    size_t nsteps = (len + 127) / 128;
    if (nsteps > 1) hx4_lm_steps(A, p, nsteps - 1);
    hx4_lm_steps(A, p + len - 128, 1);
    return hx3_lm_finish(A, s, len);
}

/* ================================================================== API */
/* hassh64 with the kernel for `level` (HX_PORTABLE .. HX_V4); the caller
 * must make sure the CPU supports it. */
static inline uint64_t hassh64_x86_level(const void *data, size_t len, uint64_t seed, int level) {
    const uint8_t *p = (const uint8_t *)data;
    if (len <= 64 || level < HX_V3) {
        if (level == HX_PORTABLE) {
            if (len <= 32) return hassh_small(p, len, seed);
            if (len <= 64) return hassh_mid(p, len, seed);
            return hassh_large_portable(p, len, seed);
        }
        return hassh64(p, len, seed);
    }
    uint64_t s = hassh_seed_s(seed);
    if (level >= HX_V4) return len < HASSH_LONG_MIN ? hx4_lanes(p, len, s, NULL) : hx4_long(p, len, s, NULL);
    return len < HASSH_LONG_MIN ? hx3_lanes(p, len, s, NULL) : hx3_long(p, len, s, NULL);
}
/* hassh64 with the best kernel this CPU supports (detected once). */
static inline uint64_t hassh64_x86(const void *data, size_t len, uint64_t seed) {
    if (len <= 64) return hassh64(data, len, seed);
    return hassh64_x86_level(data, len, seed, hx_level());
}
#endif /* HASSH_X86_H */
