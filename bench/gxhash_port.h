/* gxhash x86 path, extracted from SMHasher3 hashes/gxhash.cpp (MIT), for benchmarking only */
#include <immintrin.h>
#include <string.h>
#include <stdint.h>
#define FORCE_INLINE static inline __attribute__((always_inline))
const uint8_t KEYDATA[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x42, 0x45, 0x78, 0xf2, 0x21, 0x3e, 0x9d, 0xb0, 0xe5, 0x22, 0xc2, 0x89, 0x8e, 0xc2, 0x3b, 0xfc,
    0x79, 0xe2, 0xfc, 0x03, 0x9b, 0x2e, 0x6b, 0xcb, 0x58, 0xdc, 0x61, 0xb3, 0xd9, 0x2b, 0x13, 0x39,
    0x32, 0x2e, 0x01, 0xd0, 0x7d, 0x2b, 0x9d, 0x68, 0xb7, 0xb1, 0x44, 0x55, 0x2b, 0x12, 0x8b, 0xc7,
};
static constexpr uint64_t VECTOR_SIZE   = 16;
static constexpr uint64_t PAGE_SIZE     = 0x1000;
static constexpr uint64_t UNROLL_FACTOR = 8;


const __m128i * KEYS = (const __m128i *)&KEYDATA[1 * VECTOR_SIZE];
static_assert(sizeof(__m128i) == VECTOR_SIZE, "Code assumes VECTOR_SIZE == sizeof(__m128i)");

FORCE_INLINE bool check_same_page( const __m128i * ptr ) {
    uint64_t offset = ((uintptr_t)ptr) & (PAGE_SIZE - 1);

    return offset < (PAGE_SIZE - VECTOR_SIZE);
}

FORCE_INLINE __m128i get_partial_unsafe( const __m128i * ptr, const size_t len ) {
    __m128i indices = _mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    __m128i len_vec = _mm_set1_epi8((uint8_t)len);
    __m128i mask    = _mm_cmpgt_epi8(len_vec, indices);
    __m128i partial = _mm_and_si128(_mm_loadu_si128(ptr), mask);

    return _mm_add_epi8(partial, len_vec);
}

FORCE_INLINE __m128i get_partial_safe( const __m128i * ptr, const size_t len ) {
    uint8_t buf[VECTOR_SIZE] = { 0 };

    memcpy(buf, ptr, len);
    __m128i partial = _mm_loadu_si128((const __m128i *)buf);
    return _mm_add_epi8(partial, _mm_set1_epi8((uint8_t)len));
}

FORCE_INLINE __m128i get_partial( const __m128i * ptr, const size_t len ) {
    if (check_same_page(ptr)) {
        return get_partial_unsafe(ptr, len);
    } else {
        return get_partial_safe(ptr, len);
    }
}

FORCE_INLINE __m128i compress_8( const __m128i * ptr, const __m128i * end,
        __m128i hash_vector, const size_t len ) {
    // Disambiguation vectors
    __m128i t1 = _mm_setzero_si128();
    __m128i t2 = _mm_setzero_si128();
    // Hash is processed in two separate 128-bit parallel lanes.
    // This allows the same processing to be applied using 256-bit V-AES intrinsics
    // so that hashes are stable in both cases.
    __m128i lane1 = hash_vector;
    __m128i lane2 = hash_vector;

    while (ptr < end) {
        static_assert(UNROLL_FACTOR == 8, "Loop is coded to unroll 8 times");
        __m128i v0 = _mm_loadu_si128(ptr++);
        __m128i v1 = _mm_loadu_si128(ptr++);
        __m128i v2 = _mm_loadu_si128(ptr++);
        __m128i v3 = _mm_loadu_si128(ptr++);
        __m128i v4 = _mm_loadu_si128(ptr++);
        __m128i v5 = _mm_loadu_si128(ptr++);
        __m128i v6 = _mm_loadu_si128(ptr++);
        __m128i v7 = _mm_loadu_si128(ptr++);

        __m128i tmp1, tmp2;
        tmp1  = _mm_aesenc_si128(v0  , v2);
        tmp2  = _mm_aesenc_si128(v1  , v3);
        tmp1  = _mm_aesenc_si128(tmp1, v4);
        tmp2  = _mm_aesenc_si128(tmp2, v5);
        tmp1  = _mm_aesenc_si128(tmp1, v6);
        tmp2  = _mm_aesenc_si128(tmp2, v7);

        t1    = _mm_add_epi8(t1, _mm_loadu_si128(&KEYS[0]));
        t2    = _mm_add_epi8(t2, _mm_loadu_si128(&KEYS[1]));

        lane1 = _mm_aesenclast_si128(_mm_aesenc_si128(tmp1, t1), lane1);
        lane2 = _mm_aesenclast_si128(_mm_aesenc_si128(tmp2, t2), lane2);
    }

    // For 'Zeroes' test
    const __m128i len_vec = _mm_set1_epi32((uint32_t)len);
    lane1 = _mm_add_epi8(lane1, len_vec);
    lane2 = _mm_add_epi8(lane2, len_vec);

    return _mm_aesenc_si128(lane1, lane2);
}

FORCE_INLINE __m128i compress_many( const __m128i * ptr, const __m128i * end,
        __m128i hash_vector, const size_t len ) {
    const uint64_t  unrollable_blocks_count =  (end - ptr) / UNROLL_FACTOR;
    const __m128i * endptr = end - unrollable_blocks_count * UNROLL_FACTOR;

    // Process first individual blocks until we have a whole number of 8 blocks
    while (ptr < endptr) {
        __m128i v0 = _mm_loadu_si128(ptr++);
        hash_vector = _mm_aesenc_si128(hash_vector, v0);
    }

    // Process the remaining n * 8 blocks
    return compress_8(ptr, end, hash_vector, len);
}

FORCE_INLINE __m128i compress_all( const void * in, const size_t len ) {
    const __m128i * ptr = (const __m128i *)in;
    const __m128i * end = (const __m128i *)((const uint8_t *)in + len);
    const uint64_t  extra_bytes_count = len % VECTOR_SIZE;
    __m128i         hash_vector;

    if (len == 0) {
        return _mm_setzero_si128();
    }

    if (len <= VECTOR_SIZE) {
        return get_partial(ptr, len);
    }

    if (extra_bytes_count == 0) {
        hash_vector = _mm_loadu_si128(ptr++);
    } else {
        hash_vector = get_partial(ptr, extra_bytes_count);
        ptr         = (const __m128i *)((const uint8_t *)ptr + extra_bytes_count);
    }

    __m128i v0 = _mm_loadu_si128(ptr++);
    if (len > VECTOR_SIZE * 2) {
        // Fast path when input length > 32 and <= 48
        __m128i v = _mm_loadu_si128(ptr++);
        v0 = _mm_aesenc_si128(v0, v);

        if (len > VECTOR_SIZE * 3) {
            // Fast path when input length > 48 and <= 64
            __m128i v = _mm_loadu_si128(ptr++);
            v0 = _mm_aesenc_si128(v0, v);

            if (len > VECTOR_SIZE * 4) {
                // Input message is large and we can use the high ILP loop
                hash_vector = compress_many(ptr, end, hash_vector, len);
            }
        }
    }

    v0 = _mm_aesenc_si128(v0, _mm_loadu_si128(&KEYS[0]));
    v0 = _mm_aesenc_si128(v0, _mm_loadu_si128(&KEYS[1]));
    v0 = _mm_aesenclast_si128(hash_vector, v0);

    return v0;
}

FORCE_INLINE __m128i finalize( __m128i hash ) {
    hash = _mm_aesenc_si128(hash, _mm_loadu_si128(    &KEYS[0]));
    hash = _mm_aesenc_si128(hash, _mm_loadu_si128(    &KEYS[1]));
    hash = _mm_aesenclast_si128(hash, _mm_loadu_si128(&KEYS[2]));
    return hash;
}

template <bool output64>
FORCE_INLINE void gxhash_x86( const void * in, const size_t len, const uint64_t seed, void * out ) {
    __m128i seedx = _mm_set1_epi64x(seed);

    __m128i state = compress_all(in, len);

    state = _mm_aesenc_si128(state, seedx);
    state = finalize(state);
    if (output64) {
        { uint64_t v_ = _mm_extract_epi64(state, 0); memcpy(out, &v_, 8); }
    } else {
        _mm_storeu_si128((__m128i *)out, state);
    }
}

