/* Statistical checks for long inputs (>= 4096 bytes, the LeMac path), which
 * SMHasher3's quality tests barely reach (their keys are mostly < 4 KiB).
 *   1. Avalanche: flipping one input bit flips each output bit with p = 1/2.
 *   2. Differential: h(K)^h(K^A)^h(K^B)^h(K^A^B), with A and B in different
 *      LeMac states (or the same one), must look random; a linear combination
 *      of independent states would make it 0.
 *   3. Sparse keys: every 1- and 2-bit pattern in the first 1024 bytes of a
 *      zero key; 64-bit collisions must be absent and 32-bit ones Poisson.
 *   4. Seed avalanche on long keys.
 * Build: cc -O2 -march=native test/longstats.c -o build/longstats */
#include "../hassh.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t rs = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static void fill(uint8_t *p, size_t n) { for (size_t i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }
static int fails = 0;

/* Max |p - 1/2| over all (input bit, output bit) cells; with N samples per
 * cell the noise is 0.5/sqrt(N), so flag anything above 6 sigma. */
static void avalanche(size_t len, int nbits, int N) {
    uint8_t *k = malloc(len);
    static long cnt[256][64];
    size_t bitpos[256];
    for (int b = 0; b < nbits; b++) {           /* head, tail, step and state boundaries, random */
        size_t byte = b < 32 ? (size_t)b * 7 % 256 : b < 64 ? len - 1 - (size_t)(b - 32) * 7 % 256
                    : b < 96 ? (len - 128) + (size_t)(b - 64) * 4 : (size_t)(rnd() % len);
        bitpos[b] = byte * 8 + (rnd() & 7);
    }
    memset(cnt, 0, sizeof cnt);
    for (int t = 0; t < N; t++) {
        fill(k, len);
        uint64_t seed = rnd(), h = hassh64(k, len, seed);
        for (int b = 0; b < nbits; b++) {
            k[bitpos[b] / 8] ^= (uint8_t)(1u << (bitpos[b] % 8));
            uint64_t d = h ^ hassh64(k, len, seed);
            k[bitpos[b] / 8] ^= (uint8_t)(1u << (bitpos[b] % 8));
            for (int o = 0; o < 64; o++) cnt[b][o] += (d >> o) & 1;
        }
    }
    double worst = 0, lim = 6 * 0.5 / sqrt((double)N);
    for (int b = 0; b < nbits; b++)
        for (int o = 0; o < 64; o++) {
            double dev = fabs((double)cnt[b][o] / N - 0.5);
            if (dev > worst) worst = dev;
        }
    int bad = worst > lim;
    fails += bad;
    printf("avalanche  len %5zu: worst |p-0.5| = %.4f (limit %.4f, %d bits x 64)  %s\n", len, worst, lim, nbits,
           bad ? "FAIL" : "ok");
    free(k);
}

/* Differential: A, B single-bit differences at given byte offsets. */
static void differential(size_t len, size_t offa, size_t offb, const char *what, int N) {
    uint8_t *k = malloc(len);
    long ones[64] = {0}, zero = 0;
    for (int t = 0; t < N; t++) {
        fill(k, len);
        uint64_t seed = rnd();
        uint8_t ma = (uint8_t)(1u << (rnd() & 7)), mb = (uint8_t)(1u << (rnd() & 7));
        uint64_t d = hassh64(k, len, seed);
        k[offa] ^= ma; d ^= hassh64(k, len, seed);
        k[offb] ^= mb; d ^= hassh64(k, len, seed);
        k[offa] ^= ma; d ^= hassh64(k, len, seed);
        zero += d == 0;
        for (int o = 0; o < 64; o++) ones[o] += (d >> o) & 1;
    }
    double worst = 0, lim = 6 * 0.5 / sqrt((double)N);
    for (int o = 0; o < 64; o++) { double dev = fabs((double)ones[o] / N - 0.5); if (dev > worst) worst = dev; }
    int bad = zero > 0 || worst > lim;
    fails += bad;
    printf("diff  %-28s len %5zu: zero %ld/%d, worst bit bias %.4f  %s\n", what, len, zero, N, worst,
           bad ? "FAIL" : "ok");
    free(k);
}

static int cmp64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}
static void sparse(size_t len, size_t span) {
    uint8_t *k = calloc(len, 1);
    size_t nb = span * 8, n = 1 + nb + nb * (nb - 1) / 2, m = 0;
    uint64_t *h = malloc(n * sizeof *h), seed = rnd();
    h[m++] = hassh64(k, len, seed);
    for (size_t i = 0; i < nb; i++) {
        k[i / 8] ^= (uint8_t)(1u << (i % 8));
        h[m++] = hassh64(k, len, seed);
        for (size_t j = i + 1; j < nb; j++) {
            k[j / 8] ^= (uint8_t)(1u << (j % 8));
            h[m++] = hassh64(k, len, seed);
            k[j / 8] ^= (uint8_t)(1u << (j % 8));
        }
        k[i / 8] ^= (uint8_t)(1u << (i % 8));
    }
    qsort(h, n, sizeof *h, cmp64);
    long c64 = 0;
    for (size_t i = 1; i < n; i++) c64 += h[i] == h[i - 1];
    for (size_t i = 0; i < n; i++) h[i] >>= 32;   /* high 32 bits */
    qsort(h, n, sizeof *h, cmp64);
    long c32 = 0;
    for (size_t i = 1; i < n; i++) c32 += h[i] == h[i - 1];
    double e32 = (double)n * (n - 1) / 2 / 4294967296.0;
    int bad = c64 > 0 || fabs(c32 - e32) > 6 * sqrt(e32);
    fails += bad;
    printf("sparse     len %5zu: %zu keys (<=2 bits in %zu bytes), 64-bit collisions %ld, high-32 %ld (expected %.0f)  %s\n",
           len, n, span, c64, c32, e32, bad ? "FAIL" : "ok");
    free(h); free(k);
}

static void seedaval(size_t len, int N) {
    uint8_t *k = malloc(len);
    static long cnt[64][64];
    memset(cnt, 0, sizeof cnt);
    for (int t = 0; t < N; t++) {
        fill(k, len);
        uint64_t seed = rnd(), h = hassh64(k, len, seed);
        for (int b = 0; b < 64; b++) {
            uint64_t d = h ^ hassh64(k, len, seed ^ (1ull << b));
            for (int o = 0; o < 64; o++) cnt[b][o] += (d >> o) & 1;
        }
    }
    double worst = 0, lim = 6 * 0.5 / sqrt((double)N);
    for (int b = 0; b < 64; b++)
        for (int o = 0; o < 64; o++) { double dev = fabs((double)cnt[b][o] / N - 0.5); if (dev > worst) worst = dev; }
    int bad = worst > lim;
    fails += bad;
    printf("seed aval. len %5zu: worst |p-0.5| = %.4f (limit %.4f)  %s\n", len, worst, lim, bad ? "FAIL" : "ok");
    free(k);
}

int main(int argc, char **argv) {
    int q = argc > 1 && !strcmp(argv[1], "--quick");
    int N = q ? 4000 : 40000;
    size_t lens[] = {4096, 4097, 4224, 5000, 8192, 65536};
    for (int i = 0; i < 6; i++) avalanche(lens[i], i == 5 ? 64 : 128, i == 5 ? N / 8 : N);
    size_t L = 8192;
    differential(L, 0, 16, "state 0 / state 1, same step", N * 4);
    differential(L, 0, 32, "state 0 / state 0, same step", N * 4);
    differential(L, 5, 128 + 21, "state 0 / state 1, next step", N * 4);
    differential(L, 100, L - 1, "head / last byte", N * 4);
    differential(L, L - 128 - 3, L - 2, "last two steps", N * 4);
    differential(4100, 4100 - 125, 4100 - 129, "overlapped final step", N * 4);
    sparse(4096, q ? 64 : 256);
    sparse(4224, q ? 64 : 256);
    seedaval(4096, N / 4);
    seedaval(65536, N / 32);
    printf(fails ? "%d FAILED\n" : "all long-input statistics ok%.0d\n", fails);
    return fails != 0;
}
