// Benchmark hassh64 against xxh3-64, rapidhash v3, wyhash v4.2, gxhash-64.
// Build: see Makefile. Run pinned to one P-core: taskset -c 3 ./bench
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>

#include "../hassh.h"
#define XXH_INLINE_ALL
#include "../deps/xxHash/xxhash.h"
#include "../deps/rapidhash/rapidhash.h"
#include "../deps/wyhash/wyhash.h"
#include "gxhash_port.h"

static inline uint64_t now_ns() {
    timespec t; clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}

struct H { const char *name; uint64_t (*fn)(const void *, size_t, uint64_t); };

static uint64_t f_hassh(const void *p, size_t n, uint64_t s) { return hassh64(p, n, s); }
static uint64_t f_xxh3(const void *p, size_t n, uint64_t s) { return XXH3_64bits_withSeed(p, n, s); }
static uint64_t f_rapid(const void *p, size_t n, uint64_t s) { return rapidhash_withSeed(p, n, s); }
static uint64_t f_wy(const void *p, size_t n, uint64_t s) { return wyhash(p, n, s, _wyp); }
static uint64_t f_gx(const void *p, size_t n, uint64_t s) { uint64_t o; gxhash_x86<true>(p, n, s, &o); return o; }

static const H HS[] = {
    {"hassh64", f_hassh}, {"xxh3-64", f_xxh3}, {"rapidhash", f_rapid}, {"wyhash", f_wy}, {"gxhash-64", f_gx},
};
static const int NH = sizeof HS / sizeof HS[0];

// Templates so each hash gets fully inlined into its own loop.
template <uint64_t (*F)(const void *, size_t, uint64_t)>
static double latency_ns(const uint8_t *buf, size_t len, long iters) {
    double best = 1e30;
    for (int r = 0; r < 7; r++) {
        uint64_t h = 0, t0 = now_ns();
        for (long i = 0; i < iters; i++) h = F(buf + (h & 7), len, i);  // dependent chain
        uint64_t t1 = now_ns();
        asm volatile("" :: "r"(h));
        best = std::min(best, (double)(t1 - t0) / iters);
    }
    return best;
}
template <uint64_t (*F)(const void *, size_t, uint64_t)>
static double thru_ns(const uint8_t *buf, size_t len, long iters) {
    double best = 1e30;
    for (int r = 0; r < 7; r++) {
        uint64_t acc = 0, t0 = now_ns();
        for (long i = 0; i < iters; i++) acc += F(buf + (i & 63), len, 0);  // independent keys
        uint64_t t1 = now_ns();
        asm volatile("" :: "r"(acc));
        best = std::min(best, (double)(t1 - t0) / iters);
    }
    return best;
}

#define FOR_EACH(M) M(f_hassh) M(f_xxh3) M(f_rapid) M(f_wy) M(f_gx)

int main(int argc, char **argv) {
    std::vector<uint8_t> big(64 << 20);
    uint64_t x = 88172645463325252ull;
    for (auto &b : big) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; b = (uint8_t)x; }
    const uint8_t *buf = big.data();
    bool only_small = argc > 1 && !strcmp(argv[1], "small");

    printf("Small keys, ns/hash: latency (dependent chain) | throughput (independent)\n");
    printf("%6s", "len");
    for (int h = 0; h < NH; h++) printf(" %19s", HS[h].name);
    printf("\n");
    const size_t lens[] = {0, 3, 4, 7, 8, 12, 16, 17, 24, 32, 33, 48, 64, 65, 96, 128, 129, 200, 256, 257, 384, 512};
    for (size_t len : lens) {
        printf("%6zu", len);
        long it = 20000000;
#define LAT(F) printf("  %7.2f | %7.2f  ", latency_ns<F>(buf, len, it), thru_ns<F>(buf, len, it));
        FOR_EACH(LAT)
        printf("\n");
        fflush(stdout);
    }
    if (only_small) return 0;

    printf("\nBulk throughput, GB/s (best of 7)\n%10s", "size");
    for (int h = 0; h < NH; h++) printf(" %10s", HS[h].name);
    printf("\n");
    const size_t sizes[] = {1 << 10, 4 << 10, 16 << 10, 256 << 10, 1 << 20, 16 << 20, 64 << 20};
    for (size_t sz : sizes) {
        printf("%10zu", sz);
        long reps = std::max<long>(4, (long)((1ull << 31) / sz));
#define BULK(F) { double best = 1e30; for (int r = 0; r < 7; r++) { uint64_t a = 0, t0 = now_ns(); \
            for (long i = 0; i < reps; i++) a += F(buf, sz, i); uint64_t t1 = now_ns(); asm volatile("" :: "r"(a)); \
            best = std::min(best, (double)(t1 - t0)); } printf(" %10.1f", (double)sz * reps / best); }
        FOR_EACH(BULK)
        printf("\n");
        fflush(stdout);
    }
    return 0;
}
