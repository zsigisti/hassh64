CC  ?= gcc
CXX ?= g++
B   := build

.PHONY: test test-arm bench clean

# Cross-check: the VAES, SSE/AES-NI and portable builds must agree with the
# portable reference and with each other (same digest), and never read out of
# bounds (exact-size allocations under ASan).
test: | $(B)
	$(CC) -O2 -march=native -Wall -Wextra -o $(B)/cc_vaes  test/crosscheck.c
	$(CC) -O2 -march=native -mno-vaes -Wall -Wextra -o $(B)/cc_sse test/crosscheck.c
	$(CC) -O2 -DHASSH_PORTABLE -Wall -Wextra -o $(B)/cc_port test/crosscheck.c
	$(CC) -O1 -g -fsanitize=address,undefined -march=native -o $(B)/bounds test/bounds.c
	$(CC) -O1 -g -fsanitize=address,undefined -DHASSH_PORTABLE -o $(B)/bounds_port test/bounds.c
	@a=$$($(B)/cc_vaes) && b=$$($(B)/cc_sse) && c=$$($(B)/cc_port) && \
	 echo "vaes: $$a" && echo "sse:  $$b" && echo "port: $$c" && \
	 [ "$$a" = "$$b" ] && [ "$$b" = "$$c" ] && echo "cross-check OK"
	$(B)/bounds && $(B)/bounds_port
	$(CC) -O2 -march=native -Wall -Wextra -o $(B)/seed_vaes test/seedindep.c
	$(CC) -O2 -march=native -mno-vaes -Wall -Wextra -o $(B)/seed_sse test/seedindep.c
	$(CC) -O2 -DHASSH_PORTABLE -Wall -Wextra -o $(B)/seed_port test/seedindep.c
	$(B)/seed_vaes && $(B)/seed_sse && $(B)/seed_port
	$(CC) -O2 -march=native -Wall -Wextra -o $(B)/xlen_vaes test/xlen.c
	$(CC) -O2 -DHASSH_PORTABLE -Wall -Wextra -o $(B)/xlen_port test/xlen.c
	$(B)/xlen_vaes | tail -1 && $(B)/xlen_port | tail -1

# ARM (AESE/AESMC path) via zig cross-compile + qemu user-mode.
#   QEMU=/path/to/qemu-aarch64 make test-arm     (run `make test` first)
QEMU ?= .tools/qemu-aarch64
test-arm: | $(B)
	zig cc -target aarch64-linux-musl -mcpu=cortex_a76+aes -O2 -Wall -Wextra -o $(B)/arm_cc test/crosscheck.c
	zig cc -target aarch64-linux-musl -mcpu=cortex_a76+aes -O2 -o $(B)/arm_seed test/seedindep.c
	zig cc -target aarch64-linux-musl -mcpu=cortex_a76+aes -O1 -o $(B)/arm_bounds test/bounds.c
	@a=$$($(QEMU) -cpu max $(B)/arm_cc) && b=$$($(B)/cc_vaes) && echo "arm:  $$a" && echo "x86:  $$b" && \
	 [ "$$a" = "$$b" ] && echo "ARM matches x86"
	$(QEMU) -cpu max $(B)/arm_seed | tail -1
	$(QEMU) -cpu max $(B)/arm_bounds
	zig cc -target aarch64-linux-musl -mcpu=cortex_a76+aes -O2 -o $(B)/arm_xlen test/xlen.c
	$(QEMU) -cpu max $(B)/arm_xlen | tail -1

bench: | $(B)
	$(CXX) -O3 -march=native -std=c++17 -o $(B)/bench bench/bench.cpp
	taskset -c 3 $(B)/bench

$(B):
	mkdir -p $(B)

clean:
	rm -rf $(B)
