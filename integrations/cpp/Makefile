CXX ?= c++
B := ../../build
test:
	@mkdir -p $(B)
	@for std in 11 17 20; do \
	  $(CXX) -std=c++$$std -O2 -Wall -Wextra -Werror -pedantic -I../.. test.cpp -o $(B)/cpp_$$std && \
	  printf 'c++%s: ' $$std && $(B)/cpp_$$std || exit 1; done
.PHONY: test
