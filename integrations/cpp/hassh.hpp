// hassh64 for C++ hash containers. Header-only; needs hassh.h on the include path.
//
//   #include "hassh.hpp"
//   std::unordered_map<std::string, int, hassh::hash<std::string>> m;
//   std::unordered_set<std::string_view, hassh::hash<std::string_view>> s;
//
//   // Heterogeneous lookup (C++20): find(std::string_view) without a temporary
//   std::unordered_map<std::string, int, hassh::string_hash, std::equal_to<>> h;
//
// Strings hash their bytes, so hassh::hash<std::string>{}(s) ==
// hassh64(s.data(), s.size(), seed), the same value as the C, Rust, SQL and
// Python versions. Integers and other trivially copyable keys hash their
// object bytes (host byte order). The seed is fixed at compile time
// (HASSH_CPP_SEED, default 0); pass one to the constructor for a per-container
// seed. hassh64 is not a defence against hash flooding from untrusted keys
// with a known seed (see README, Security).
#ifndef HASSH_HPP
#define HASSH_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#if __cplusplus >= 201703L
#include <string_view>
#endif
#include "hassh.h"

#ifndef HASSH_CPP_SEED
#define HASSH_CPP_SEED 0
#endif

namespace hassh {

inline std::uint64_t hash_bytes(const void *p, std::size_t n, std::uint64_t seed = HASSH_CPP_SEED) {
    return hassh64(p, n, seed);
}

template <class T, class Enable = void>
struct hash {
    static_assert(std::is_trivially_copyable<T>::value && !std::is_pointer<T>::value,
                  "hassh::hash<T>: T must be a string type or trivially copyable (not a pointer)");
    std::uint64_t seed;
    hash() noexcept : seed(HASSH_CPP_SEED) {}
    explicit hash(std::uint64_t s) noexcept : seed(s) {}
    std::size_t operator()(const T &v) const noexcept {
        return static_cast<std::size_t>(hassh64(&v, sizeof v, seed));
    }
};

template <class C, class Tr, class A>
struct hash<std::basic_string<C, Tr, A>> {
    std::uint64_t seed;
    hash() noexcept : seed(HASSH_CPP_SEED) {}
    explicit hash(std::uint64_t s) noexcept : seed(s) {}
    std::size_t operator()(const std::basic_string<C, Tr, A> &v) const noexcept {
        return static_cast<std::size_t>(hassh64(v.data(), v.size() * sizeof(C), seed));
    }
};

#if __cplusplus >= 201703L
template <class C, class Tr>
struct hash<std::basic_string_view<C, Tr>> {
    std::uint64_t seed;
    hash() noexcept : seed(HASSH_CPP_SEED) {}
    explicit hash(std::uint64_t s) noexcept : seed(s) {}
    std::size_t operator()(std::basic_string_view<C, Tr> v) const noexcept {
        return static_cast<std::size_t>(hassh64(v.data(), v.size() * sizeof(C), seed));
    }
};

// Transparent string hasher: std::string, std::string_view and const char*
// keys hash alike, for heterogeneous lookup with std::equal_to<>.
struct string_hash {
    using is_transparent = void;
    std::uint64_t seed;
    string_hash() noexcept : seed(HASSH_CPP_SEED) {}
    explicit string_hash(std::uint64_t s) noexcept : seed(s) {}
    std::size_t operator()(std::string_view v) const noexcept {
        return static_cast<std::size_t>(hassh64(v.data(), v.size(), seed));
    }
};
#endif

}  // namespace hassh

#endif
