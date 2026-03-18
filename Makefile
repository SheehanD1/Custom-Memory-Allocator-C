# ============================================================
# Custom Memory Allocator — Makefile
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g -Iinclude
LDFLAGS =

# Build directory
BUILD   = build

# Source files
SRC_DIR = src
SRC     = $(wildcard $(SRC_DIR)/*.c)
OBJ     = $(patsubst $(SRC_DIR)/%.c, $(BUILD)/%.o, $(SRC))

# Test files
TEST_DIR   = tests
TEST_SRC   = $(wildcard $(TEST_DIR)/*.c)
TEST_BIN   = $(patsubst $(TEST_DIR)/%.c, $(BUILD)/%, $(TEST_SRC))

# Benchmark files
BENCH_DIR  = benchmarks
BENCH_SRC  = $(wildcard $(BENCH_DIR)/*.c)
BENCH_BIN  = $(patsubst $(BENCH_DIR)/%.c, $(BUILD)/%, $(BENCH_SRC))
BENCH_FLAGS = -O2

# Optional: compile with best-fit strategy
# Usage: make all STRATEGY=best-fit
ifeq ($(STRATEGY),best-fit)
  CFLAGS += -DUSE_BEST_FIT
endif

# ============================================================
# Targets
# ============================================================

.PHONY: all clean test bench

all: $(OBJ)
	@echo "Build complete."

# Compile source objects
$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executables (each test links against all source objects)
$(BUILD)/%: $(TEST_DIR)/%.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LDFLAGS)

# Build benchmark executables
$(BUILD)/%: $(BENCH_DIR)/%.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $(BENCH_FLAGS) $< $(OBJ) -o $@ $(LDFLAGS)

# Create build directory
$(BUILD):
	mkdir -p $(BUILD)

# Run all tests
test: $(TEST_BIN)
	@echo "========================================"
	@echo "         Running All Tests              "
	@echo "========================================"
	@for t in $(TEST_BIN); do \
		echo "\n--- Running $$t ---"; \
		./$$t; \
	done

# Run benchmarks
bench: $(BENCH_BIN)
	@echo "========================================"
	@echo "       Running Benchmarks               "
	@echo "========================================"
	@for b in $(BENCH_BIN); do \
		echo "\n--- Running $$b ---"; \
		./$$b; \
	done

# Clean build artifacts
clean:
	rm -rf $(BUILD)
