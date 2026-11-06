/* Compiled the way kernels compile (see Makefile): must build with no
 * undefined symbols and no SIMD/FPU instructions. */
#include "hassh.h"

uint64_t name_hash(const char *name, size_t len) { return hassh64(name, len, 0x6b65726e656c0001ull); }
uint64_t u64_hash(uint64_t key) { return hassh64(&key, sizeof key, 7); }

static hassh64_key big_key;
void big_key_init(uint64_t seed) { hassh64_key_init(&big_key, seed); }
uint64_t big_hash(const void *p, size_t len) { return hassh64_keyed(&big_key, p, len); }
