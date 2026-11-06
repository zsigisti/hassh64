/* hassh64_init/update/digest must equal hassh64() for every length and every
 * way of splitting the input, including digests taken mid-stream. Run for
 * each code path (see Makefile) and under ASan. */
#include "../hassh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng = 0x9e3779b97f4a7c15ull;
static uint64_t next(void) {
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return rng;
}

/* Feed m in pieces of size step (0 = random sizes up to 300, -1 = random up
 * to 9000) and compare the digest after every piece with the one-shot hash
 * of the prefix when check_prefixes is set. */
static int run(const uint8_t *m, size_t len, uint64_t seed, long step, int check_prefixes) {
    hassh64_state st;
    hassh64_init(&st, seed);
    size_t off = 0;
    while (off < len) {
        size_t n = step > 0 ? (size_t)step : (size_t)(next() % (step == 0 ? 300 : 9000));
        if (n > len - off) n = len - off;
        hassh64_update(&st, m + off, n);
        off += n;
        if (check_prefixes && hassh64_digest(&st) != hassh64(m, off, seed)) {
            printf("FAIL prefix len=%zu step=%ld at %zu\n", len, step, off);
            return 1;
        }
    }
    if (hassh64_digest(&st) != hassh64(m, len, seed)) {
        printf("FAIL len=%zu seed=%llu step=%ld\n", len, (unsigned long long)seed, step);
        return 1;
    }
    return 0;
}

int main(void) {
    const size_t max = 20000;
    uint8_t *m = (uint8_t *)malloc(max);
    for (size_t i = 0; i < max; i++) m[i] = (uint8_t)next();
    static const long steps[] = { 1, 3, 16, 63, 64, 65, 127, 128, 129, 255, 256, 4095, 4096, 4097, 8191, 0, -1 };
    const size_t nsteps = sizeof steps / sizeof steps[0];
    int fail = 0;
    size_t cases = 0;
    for (size_t len = 0; len <= max && !fail; len++) {
        /* every length near the path boundaries, a sample elsewhere */
        int near = len <= 600 || (len >= 3900 && len <= 4500) || (len >= 8000 && len <= 8400) || len >= 19800;
        if (!near && len % 97) continue;
        for (size_t s = 0; s < nsteps && !fail; s++) {
            if (steps[s] == 1 && len > 9000) continue;   /* slow, adds nothing new */
            fail |= run(m, len, len * 0x100000001b3ull, steps[s], 0);
            cases++;
        }
    }
    /* digests between updates leave the state usable */
    for (size_t len = 4000; len <= 4400 && !fail; len += 37) fail |= run(m, len, 7, 0, 1);
    fail |= run(m, max, 7, -1, 1);
    /* the state can be copied and continued */
    if (!fail) {
        hassh64_state a, b;
        hassh64_init(&a, 99);
        hassh64_update(&a, m, 10000);
        b = a;
        hassh64_update(&a, m + 10000, 5000);
        hassh64_update(&b, m + 10000, 5000);
        if (hassh64_digest(&a) != hassh64(m, 15000, 99) || hassh64_digest(&b) != hassh64_digest(&a)) {
            puts("FAIL copied state");
            fail = 1;
        }
    }
    free(m);
    if (!fail) printf("stream: %zu cases match one-shot\n", cases);
    return fail;
}
