# MPMC queue tests. Shared test framework: ../tests/vendor (Acutest).
MPMC_DIR := .
# Acutest and other shared test headers live under System/tests/
SYSTEM_TEST_VENDOR := ../tests/vendor

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -g
LDFLAGS ?= -pthread
CPPFLAGS := -I$(MPMC_DIR) -I$(SYSTEM_TEST_VENDOR)

TEST_SRC  := tests/test_mpmc.c
TEST_BIN  := tests/test_mpmc_queue
BENCH_SRC := tests/bench_mpmc.c
BENCH_BIN := tests/bench_mpmc
MPMC_SRC  := $(MPMC_DIR)/mpmc.c
# Override for quicker runs, e.g.: make bench BENCH_ARGS='2000000 4096 8 8'
BENCH_ARGS ?=

.PHONY: all test bench clean

all: $(TEST_BIN) $(BENCH_BIN)

$(TEST_BIN): $(MPMC_SRC) $(TEST_SRC) $(SYSTEM_TEST_VENDOR)/acutest.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(MPMC_SRC) $(TEST_SRC) $(LDFLAGS)

# Throughput benchmark: compile with -O3 (override CFLAGS for this binary only).
$(BENCH_BIN): $(MPMC_SRC) $(BENCH_SRC)
	$(CC) -I$(MPMC_DIR) -std=c11 -Wall -Wextra -O3 -o $@ $(MPMC_SRC) $(BENCH_SRC) $(LDFLAGS)

# --no-exec: run tests in-process (fork + threads is unsafe on POSIX).
test: $(TEST_BIN)
	$(TEST_BIN) --no-exec

bench: $(BENCH_BIN)
	$(BENCH_BIN) $(BENCH_ARGS)

clean:
	rm -f $(TEST_BIN) $(BENCH_BIN)
