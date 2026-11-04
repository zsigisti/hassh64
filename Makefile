CC  ?= gcc
CXX ?= g++
B   := build

.PHONY: test test-arm test-cross bench cli clean

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
	$(CC) -O2 -march=native -o $(B)/vec_vaes test/vectors.c && $(B)/vec_vaes
	$(CC) -O2 -march=native -mno-vaes -o $(B)/vec_sse test/vectors.c && $(B)/vec_sse
	$(CC) -O2 -o $(B)/vec_dispatch test/vectors.c && $(B)/vec_dispatch
	$(CC) -O2 -DHASSH_PORTABLE -o $(B)/vec_port test/vectors.c && $(B)/vec_port
	$(CC) -std=c99 -pedantic -Wall -Wextra -Werror -c -o /dev/null test/vectors.c
	$(CXX) -std=c++11 -pedantic -Wall -Wextra -Werror -x c++ -c -o /dev/null test/vectors.c

# Other platforms via zig cross-compile + qemu user-mode (set QEMU_DIR to a
# directory holding qemu-s390x, qemu-arm, qemu-aarch64): big-endian, 32-bit.
QEMU_DIR ?= .tools
test-cross: | $(B)
	zig cc -target x86-linux-musl -O2 -o $(B)/v_i386 test/vectors.c && $(B)/v_i386
	zig cc -target x86_64-linux-musl -O2 -o $(B)/v_musl test/vectors.c && $(B)/v_musl
	zig cc -target s390x-linux-musl -O2 -o $(B)/v_s390x test/vectors.c && $(QEMU_DIR)/qemu-s390x $(B)/v_s390x
	zig cc -target arm-linux-musleabihf -O2 -o $(B)/v_arm test/vectors.c && $(QEMU_DIR)/qemu-arm $(B)/v_arm
	zig cc -target aarch64-linux-musl -O2 -o $(B)/v_a64 test/vectors.c && $(QEMU_DIR)/qemu-aarch64 $(B)/v_a64
	zig cc -target aarch64-linux-musl -mcpu=cortex_a76+aes -O2 -o $(B)/v_a64aes test/vectors.c && $(QEMU_DIR)/qemu-aarch64 -cpu max $(B)/v_a64aes
	zig cc -target riscv64-linux-musl -O2 -c -o $(B)/v_rv64.o test/vectors.c
	zig cc -target wasm32-wasi -O2 -c -o $(B)/v_wasm.o test/vectors.c
	zig cc -target x86_64-windows-gnu -O2 -c -o $(B)/v_mingw.o test/vectors.c

# ARM (AESE/AESMC path) via zig cross-compile + qemu user-mode.
#   QEMU=/path/to/qemu-aarch64 make test-arm     (run `make test` first)
QEMU ?= $(QEMU_DIR)/qemu-aarch64
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

# Command-line tool: build/hassh64 "text", or -f FILE, or stdin.
cli: | $(B)
	$(CC) -O2 -Wall -Wextra -o $(B)/hassh64 tools/hassh64.c

bench: | $(B)
	$(CXX) -O3 -march=native -std=c++17 -o $(B)/bench bench/bench.cpp
	taskset -c 3 $(B)/bench

$(B):
	mkdir -p $(B)

clean:
	rm -rf $(B)
