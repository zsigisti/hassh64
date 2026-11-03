# hassh64

> **AI-generated.** Designed and written by an AI (Anthropic's Claude, via Claude Code) under
> human direction, and reviewed only by AI agents.

A fast, seeded, non-cryptographic 64-bit hash in one C header.

```c
#include "hassh.h"
uint64_t h = hassh64(data, len, seed);
```

Not for passwords, MACs, signatures, or keys an attacker controls. MIT licensed.
