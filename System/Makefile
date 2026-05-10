# Shared unit-test vendor code lives in tests/vendor/ (e.g. Acutest).
# Each component keeps its own tests/ and Makefile; delegate here to run all.

.PHONY: test test-lock-free-queue bench bench-lock-free-queue

test-lock-free-queue:
	$(MAKE) -C lock-free-queue test

bench-lock-free-queue:
	$(MAKE) -C lock-free-queue bench

test: test-lock-free-queue

bench: bench-lock-free-queue
