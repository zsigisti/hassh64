#include "hassh_x86.h"
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>
static int LVL;
static uint64_t f_lvl(const uint8_t *p, size_t n, uint64_t s) { return hassh64_x86_level(p, n, s, LVL); }
static uint64_t f_hdr(const uint8_t *p, size_t n, uint64_t s) { return hassh64(p, n, s); }
typedef uint64_t (*fn)(const uint8_t *, size_t, uint64_t);
static double best(fn f, const uint8_t *b, size_t n, int chain) {
    double m = 1e18; long it = 20000000 / (n + 64) + 10;
    for (int r = 0; r < 9; r++) {
        uint64_t h = 0; unsigned long long t = __rdtsc();
        if (chain) for (long i = 0; i < it; i++) h = f(b + (h & 7), n, i);
        else for (long i = 0; i < it; i++) h += f(b, n, i);
        double d = (double)(__rdtsc() - t) / it; if (d < m) m = d; if (h == 42) puts("");
    }
    return m;
}
int main(void) {
    uint8_t *b = aligned_alloc(64, 1 << 21); for (int i = 0; i < 1 << 21; i++) b[i] = (uint8_t)i;
    size_t sz[] = {65, 128, 256, 1024, 4095, 4096, 16384, 65536, 1 << 20};
    printf("bytes/TSC tick, throughput | latency (unaligned chain)\n%8s %15s %15s %15s\n", "len", "header", "x86 v2", "x86 v3");
    for (int k = 0; k < 9; k++) {
        size_t n = sz[k];
        LVL = HX_V2; double v2t = best(f_lvl, b, n, 0), v2l = best(f_lvl, b, n, 1);
        LVL = HX_V3; double v3t = best(f_lvl, b, n, 0), v3l = best(f_lvl, b, n, 1);
        double ht = best(f_hdr, b, n, 0), hl = best(f_hdr, b, n, 1);
        printf("%8zu %6.1f | %6.1f %6.1f | %6.1f %6.1f | %6.1f\n", n, n / ht, n / hl, n / v2t, n / v2l, n / v3t, n / v3l);
    }
}
