# hassh64 for C++ hash containers

`hassh.hpp` (header-only, C++11 and later; needs `hassh.h` on the include path):

```cpp
#include "hassh.hpp"

std::unordered_map<std::string, int, hassh::hash<std::string>> counts;
std::unordered_set<std::uint64_t, hassh::hash<std::uint64_t>> ids;

// C++20 heterogeneous lookup: find a std::string key with a string_view, no copy
std::unordered_map<std::string, int, hassh::string_hash, std::equal_to<>> m;
m.find(std::string_view("key"));
```

* `std::string` / `std::string_view` keys hash their bytes:
  `hassh::hash<std::string>{}(s) == hassh64(s.data(), s.size(), 0)`, the same
  value as the C, Rust, SQL and Python versions.
* Other trivially copyable keys (integers, PODs) hash their object bytes, in
  host byte order. Pointers are rejected at compile time.
* The default seed is `HASSH_CPP_SEED` (0 unless you define it); pass a seed
  to the hasher's constructor for a per-container seed.

`make test` builds the tests as C++11, 17 and 20 with `-Wall -Wextra -Werror -pedantic`.
