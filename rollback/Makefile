# Rollback core: build and test with `make test` (gcc or clang, Linux; no other dependencies).
CC      ?= cc
CFLAGS  ?= -O2 -g
CSTD    := -std=c99 -D_POSIX_C_SOURCE=199309L
WARN    := -Wall -Wextra -Wpedantic -Wshadow
INC     := -Iinclude -Itests
BUILD   := build

LIB_SRC  := src/rollback.c src/rb_net.c src/rb_pages.c
TEST_SRC := tests/test_main.c tests/test_core.c tests/test_net.c tests/test_pages.c tests/toygame.c tests/simnet.c tests/harness.c
BENCH_SRC := tests/bench.c tests/toygame.c tests/simnet.c tests/harness.c

.PHONY: all test bench asan clean
all: test

$(BUILD)/test_rollback: $(LIB_SRC) $(TEST_SRC) $(wildcard include/*.h tests/*.h) | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(CFLAGS) $(INC) $(LIB_SRC) $(TEST_SRC) -o $@

$(BUILD)/bench_rollback: $(LIB_SRC) $(BENCH_SRC) $(wildcard include/*.h tests/*.h) | $(BUILD)
	$(CC) $(CSTD) $(WARN) $(CFLAGS) $(INC) $(LIB_SRC) $(BENCH_SRC) -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: $(BUILD)/test_rollback
	./$(BUILD)/test_rollback

bench: $(BUILD)/bench_rollback
	./$(BUILD)/bench_rollback

# the same tests under AddressSanitizer + UBSan
asan: | $(BUILD)
	$(CC) $(CSTD) $(WARN) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(INC) $(LIB_SRC) $(TEST_SRC) -o $(BUILD)/test_rollback_asan
	./$(BUILD)/test_rollback_asan

clean:
	rm -rf $(BUILD)
