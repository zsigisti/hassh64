CC  ?= gcc
CXX ?= g++
B   := build

.PHONY: test clean

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

$(B):
	mkdir -p $(B)

clean:
	rm -rf $(B)
