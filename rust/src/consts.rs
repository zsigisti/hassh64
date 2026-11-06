//! Nothing-up-my-sleeve constants: the SHA-512 IV, the first 64 SHA-512 round
//! constants, and their continuation (cube roots of the 81st..152nd primes).

pub(crate) const P: [u64; 8] = [
    0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
    0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
];

/// 128-bit keys as little-endian 64-bit halves (lo, hi): 0..15 lane start values,
/// 16..19 block round keys, 24..26 finalizer keys, 30..31 lane-start round keys.
pub(crate) const K: [u64; 64] = [
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
    0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
];

/// Round keys used by the AES section.
pub(crate) const ROUND_KEY0: usize = 16; // perm: keys 16..18
pub(crate) const ROUNDS: usize = 3;
pub(crate) const FIN_KEY0: usize = 24; // fin: keys 24..26
pub(crate) const FIN_ROUNDS: usize = 3;
pub(crate) const INIT_KEY0: usize = 30;
pub(crate) const INIT_KEY1: usize = 31;

/// Inputs of at least this many bytes use the LeMac absorber instead of the lanes.
pub(crate) const LONG_MIN: usize = 4096;
/// LeMac keys (lo, hi halves): 0..17 start the 18 state words, 18..35 key their
/// compression. Fractional parts of the cube roots of the 81st..152nd primes.
pub(crate) const L: [u64; 72] = [
    0x7ba0ea2d98160007, 0x7eabf2d0c21f964a, 0x8dbe8d038b409545, 0x90bb1721582e8285,
    0x99a2ad45936d4e61, 0x9f86e289fe03e739, 0xa84c4472faa9a82f, 0xb3df34fce89e0532,
    0xb99bb8d7b173534f, 0xbc76cbab1aea1f9c, 0xc226a69a780f3cc3, 0xd304f19aa233957d,
    0xde1be20a212129dd, 0xe39bb43755141950, 0xee84927cea48ddd2, 0xf3edd2773c523b67,
    0xfbfdfe53a8d32f2a, 0x0bee2c7ab77e9e25, 0x0e90181cf1b09e56, 0x25f57204c725bed8,
    0x2da45582cd598b32, 0x3a52c34c203bfcf3, 0x41dc0172cd1991c1, 0x495796fcb33cc1c0,
    0x4bd31fc693f9f16e, 0x533cde2115f5a9a0, 0x5f7abfe36e99c1d3, 0x66c206b310a57e6f,
    0x6dfcc6bc39603f61, 0x7062f20f86fd1052, 0x778d51277adec865, 0x7eaba3cc25da7048,
    0x8363eccc37a5be05, 0x85be1c253beba54e, 0x93c04028f348bbc5, 0x9f4a205fd05b2148,
    0xa19535651ca6d2de, 0xa627bb0fbf027bc7, 0xacfa80891da2f06b, 0xb3c29b23031a7f9d,
    0xb602f6fac7d3d74d, 0xc36cee0a10c7ba49, 0xc7dc81eea9ebad4f, 0xce7b8471b0f809df,
    0xd740288c84df269c, 0xe21dba7ac2290607, 0xeabbff66be175964, 0xf56a9e60f62cea92,
    0xfde41d729d126eab, 0x0434d0970e42e781, 0x0a7cb752a3f1cd86, 0x0ea7d22d6bcd7382,
    0x16f2987f9495a5ee, 0x1d20cdcd45b8de1e, 0x213af85a39b0c320, 0x2964505c52a2f35b,
    0x2d738e114181e082, 0x3b8cea0e71c58aaf, 0x4584e6ae9f54016e, 0x515f4356903dccc2,
    0x5356112ddfd5a8e9, 0x5d1bc3edbe2c897a, 0x5f0da9f8ed53548b, 0x62ef0be4d5492e78,
    0x64de896eace0be7f, 0x6e801ba3078ae05f, 0x7bdb3595cdadf50a, 0x7fa5377856834c98,
    0x818916bad3d008a8, 0x854e959f834021a7, 0x926a82c27137e2c6, 0x9622c7ba7d179197,
];
