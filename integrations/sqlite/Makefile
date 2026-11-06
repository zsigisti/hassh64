# Builds hassh64.so (hassh64.dylib on macOS) and checks it against the C vectors.
CC ?= cc
EXT := $(if $(filter Darwin,$(shell uname -s)),dylib,so)

hassh64.$(EXT): hassh64_sqlite.c ../../hassh.h
	$(CC) -O2 -fPIC -shared -Wall -Wextra -o $@ hassh64_sqlite.c

test: hassh64.$(EXT)
	@out=$$(sqlite3 :memory: < test.sql) && echo "$$out" && [ "$$out" = "sqlite: ok" ]

clean:
	rm -f hassh64.so hassh64.dylib
.PHONY: test clean
