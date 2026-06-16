#!/bin/bash
# ============================================================
# build_and_test.sh — Build & Test Script for WSL/Linux
#
# Usage: ./build_and_test.sh
#
# This script compiles the allocator, runs all tests, and
# reports results. Exit code 0 = all tests passed.
# ============================================================

set -e  # Exit on first error

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "  Custom Memory Allocator — Build & Test"
echo "========================================"
echo ""

# --- Step 1: Clean previous build ---
echo -e "${YELLOW}[1/4] Cleaning previous build...${NC}"
make clean 2>/dev/null || true
echo "      Done."
echo ""

# --- Step 2: Build the allocator ---
echo -e "${YELLOW}[2/4] Building allocator (first-fit)...${NC}"
make all
echo ""

# --- Step 3: Build and run all tests ---
echo -e "${YELLOW}[3/4] Building and running tests...${NC}"
echo ""

PASS=0
FAIL=0

for test_src in tests/test_*.c; do
    test_name=$(basename "$test_src" .c)
    echo -e "  Building ${test_name}..."
    make "build/${test_name}" 2>&1

    echo -e "  Running ${test_name}..."
    if ./build/"${test_name}"; then
        echo -e "  ${GREEN}✓ ${test_name} PASSED${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}✗ ${test_name} FAILED${NC}"
        FAIL=$((FAIL + 1))
    fi
    echo ""
done

# --- Step 4: Summary ---
echo "========================================"
echo "  Results"
echo "========================================"
TOTAL=$((PASS + FAIL))
echo -e "  Test suites passed: ${GREEN}${PASS}${NC} / ${TOTAL}"

if [ "$FAIL" -gt 0 ]; then
    echo -e "  ${RED}${FAIL} test suite(s) FAILED${NC}"
    echo "========================================"
    exit 1
else
    echo -e "  ${GREEN}All tests passed!${NC}"
    echo "========================================"
    exit 0
fi
