# Reactor tests / bench. Shared vendor: ../tests/vendor (Acutest).
REACTOR_DIR := .
SYSTEM_TEST_VENDOR := ../tests/vendor

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -g
LDFLAGS ?=
CPPFLAGS := -I$(REACTOR_DIR) -I$(SYSTEM_TEST_VENDOR)

REACTOR_SRC := $(REACTOR_DIR)/reactor.c
TEST_SRC    := tests/test_reactor.c
TEST_BIN    := tests/test_reactor
BENCH_SRC   := tests/bench_reactor.c
BENCH_BIN   := tests/bench_reactor

# Override for bench, e.g.: make bench BENCH_ARGS='200000 64'
BENCH_ARGS ?=

# Long / aggressive stress (override pieces without editing the file), e.g.:
#   make stress STRESS_DURATION=5 STRESS_FLOOD=500000 STRESS_MUX_ITERS=200000 STRESS_MUX_CONN=128
STRESS_DURATION ?= 20
STRESS_FLOOD ?= 5000000
STRESS_MUX_ITERS ?= 1500000
STRESS_MUX_CONN ?= 384

.PHONY: all test bench clean stress

all: $(TEST_BIN) $(BENCH_BIN)

$(TEST_BIN): $(REACTOR_SRC) $(TEST_SRC) $(SYSTEM_TEST_VENDOR)/acutest.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(REACTOR_SRC) $(TEST_SRC) $(LDFLAGS)

$(BENCH_BIN): $(REACTOR_SRC) $(BENCH_SRC)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -O3 -g -o $@ $(REACTOR_SRC) $(BENCH_SRC) $(LDFLAGS)

# --no-exec: Acutest runs cases in-process (fork + threads is unsafe on POSIX).
test: $(TEST_BIN)
	$(TEST_BIN) --no-exec

bench: $(BENCH_BIN)
	$(BENCH_BIN) $(BENCH_ARGS)

stress: $(BENCH_BIN)
	@echo "=== stress: duration $(STRESS_DURATION)s payload=1024 ==="
	$(BENCH_BIN) duration $(STRESS_DURATION) 1024
	@echo "=== stress: flood $(STRESS_FLOOD) rounds payload=1024 ==="
	$(BENCH_BIN) flood $(STRESS_FLOOD) 1024
	@echo "=== stress: mux $(STRESS_MUX_ITERS) rounds payload=1024 conns=$(STRESS_MUX_CONN) ==="
	$(BENCH_BIN) mux $(STRESS_MUX_ITERS) 1024 $(STRESS_MUX_CONN)

clean:
	rm -f $(TEST_BIN) $(BENCH_BIN)
