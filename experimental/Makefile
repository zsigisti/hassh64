# Experimental x86-64 kernels (v2 / v3 / v4) with run-time selection.
#   make test       every level this CPU has, against the portable reference
#   make test-sde   v4 (AVX-512) under Intel SDE: SDE=/path/to/sde64
#   make bench      default-flags build: stable header vs. the dispatched kernels
CC  ?= gcc
SDE ?= ../.tools/sde/sde64
B   := ../build/exp
CFLAGS := -O2 -Wall -Wextra

.PHONY: test test-sde bench
$(B):
	mkdir -p $(B)
test: | $(B)
	$(CC) $(CFLAGS) -o $(B)/test test.c && $(B)/test
	clang $(CFLAGS) -o $(B)/test_clang test.c && $(B)/test_clang
test-sde: | $(B)
	$(CC) $(CFLAGS) -o $(B)/test test.c
	$(SDE) -spr -- $(B)/test 4
	$(SDE) -icx -- $(B)/test 4
	$(SDE) -gnr -- $(B)/test 4
	$(SDE) -hsw -- $(B)/test        # Haswell: no VAES, must pick v2
bench: | $(B)
	$(CC) -O2 -o $(B)/bench bench.c && taskset -c 3 $(B)/bench
