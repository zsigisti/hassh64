#include "hassh.hpp"
#include <cstdio>
#include <unordered_map>
#include <unordered_set>

int main() {
    int fail = 0;
    auto check = [&](const char *what, bool ok) { if (!ok) { std::printf("FAIL %s\n", what); fail = 1; } };
    hassh::hash<std::string> hs;
    check("hello", hs("hello") == static_cast<std::size_t>(0xedfe40b18d046dbaull));
    check("seeded", hassh::hash<std::string>(42)("hello") == static_cast<std::size_t>(9390855935162765991ull));
    std::string big;
    for (int i = 0; i < 1000; i++) big += "0123456789";
    check("10000 bytes", hs(big) == static_cast<std::size_t>(6070299042740712857ull));
    std::uint64_t x = 12345;
    check("integer = its bytes", hassh::hash<std::uint64_t>{}(x) == static_cast<std::size_t>(hassh64(&x, 8, 0)));
#if __cplusplus >= 201703L
    check("string_view", hassh::hash<std::string_view>{}(std::string_view("hello")) == hs("hello"));
#endif
#if __cplusplus >= 202002L
    std::unordered_map<std::string, int, hassh::string_hash, std::equal_to<>> m{{"alpha", 1}, {"beta", 2}};
    check("heterogeneous find", m.find(std::string_view("beta")) != m.end() && m.find("gamma") == m.end());
#endif
    std::unordered_map<std::string, int, hassh::hash<std::string>> m2;
    for (int i = 0; i < 10000; i++) m2[std::to_string(i)] = i;
    check("map", m2.size() == 10000 && m2["4242"] == 4242);
    std::unordered_set<std::uint32_t, hassh::hash<std::uint32_t>> s{1, 2, 3};
    check("set", s.count(2) == 1 && s.count(4) == 0);
    std::printf(fail ? "cpp: FAIL\n" : "cpp: ok\n");
    return fail;
}
