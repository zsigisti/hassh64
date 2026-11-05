/* hassh64 — wrapper for SMHasher3. MIT licensed. */
#include "Platform.h"
#include "Hashlib.h"
#include "../../../hassh.h"  /* path from deps/smhasher3/hashes/ */

template <bool bswap>
static void hassh64_smh( const void * in, const size_t len, const seed_t seed, void * out ) {
    uint64_t h = hassh64(in, len, (uint64_t)seed);
    PUT_U64<bswap>(h, (uint8_t *)out, 0);
}

REGISTER_FAMILY(hassh,
   $.src_url    = "https://github.com/zsigisti/hassh64",
   $.src_status = HashFamilyInfo::SRC_ACTIVE
 );

REGISTER_HASH(hassh_64,
   $.desc            = "hassh64 2.0 (AES + 128-bit multiply)",
   $.hash_flags      = FLAG_HASH_AES_BASED | FLAG_HASH_ENDIAN_INDEPENDENT,
   $.impl_flags      = FLAG_IMPL_LICENSE_MIT | FLAG_IMPL_CANONICAL_LE | FLAG_IMPL_MULTIPLY_64_128,
   $.bits            = 64,
   $.verification_LE = 0x8628ECA5,
   $.verification_BE = 0x871FFBFF,
   $.hashfn_native   = hassh64_smh<false>,
   $.hashfn_bswap    = hassh64_smh<true>
 );
