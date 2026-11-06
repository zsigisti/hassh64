# Freestanding checks: kernel_check.c and the 548 vectors, built the way
# kernels build (-ffreestanding -nostdinc, no SIMD/FPU registers), for
# x86-64, AArch64 and RISC-V. The object files must have no undefined symbols
# (no memcpy, no libgcc) and the x86-64 one no SSE/AES instructions.
CC ?= cc
CLANG ?= clang
B := ../../build/kernel
INC := -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I../..
CINC := -nostdinc -isystem $(shell $(CLANG) -print-resource-dir)/include -I../..
X86 := -O2 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel -mgeneral-regs-only -Wall -Wextra -Werror
WARN := -O2 -ffreestanding -fno-stack-protector -Wall -Wextra -Werror

check: | $(B)
	$(CC) $(X86) $(INC) -c kernel_check.c -o $(B)/x86.o
	@u=$$(nm -u $(B)/x86.o); test -z "$$u" || { echo "undefined symbols: $$u"; exit 1; }
	@if objdump -d $(B)/x86.o | grep -E -q '%[xyz]mm|aes'; then echo "SIMD instructions in kernel build"; exit 1; fi
	$(CLANG) --target=aarch64-none-elf -mgeneral-regs-only $(WARN) $(CINC) -c kernel_check.c -o $(B)/a64.o
	@u=$$(nm -u $(B)/a64.o); test -z "$$u" || { echo "undefined symbols: $$u"; exit 1; }
	$(CLANG) --target=riscv64-none-elf -march=rv64imac -mabi=lp64 $(WARN) $(CINC) -c kernel_check.c -o $(B)/rv64.o
	@u=$$(nm -u $(B)/rv64.o); test -z "$$u" || { echo "undefined symbols: $$u"; exit 1; }
	$(CLANG) --target=i686-none-elf -mgeneral-regs-only $(WARN) $(CINC) -c kernel_check.c -o $(B)/i686.o
	@u=$$(nm -u $(B)/i686.o); test -z "$$u" || { echo "undefined symbols: $$u"; exit 1; }
	$(CLANG) --target=riscv32-none-elf -march=rv32imac -mabi=ilp32 $(WARN) $(CINC) -c kernel_check.c -o $(B)/rv32.o
	@u=$$(nm -u $(B)/rv32.o); test -z "$$u" || { echo "undefined symbols: $$u"; exit 1; }
	$(CLANG) --target=armv7a-none-eabi -mfloat-abi=soft $(WARN) $(CINC) -c kernel_check.c -o $(B)/armv7.o
	@u=$$(nm -u $(B)/armv7.o); test -z "$$u" || { echo "undefined symbols: $$u"; exit 1; }
	$(CC) -O2 -ffreestanding -mgeneral-regs-only -mno-red-zone -o $(B)/vectors ../../test/vectors.c
	$(B)/vectors
	@echo "kernel: ok"

$(B):
	mkdir -p $@

.PHONY: check
