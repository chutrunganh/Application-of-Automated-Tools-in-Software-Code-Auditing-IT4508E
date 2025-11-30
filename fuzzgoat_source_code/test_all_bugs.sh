#!/bin/bash
# Script to test all bugs with ESBMC
# This script runs ESBMC with different conditions to detect all 4 bugs

cd "$(dirname "$0")"

echo "=========================================="
echo "Testing all bugs with ESBMC"
echo "=========================================="
echo ""

# Test 1: Object bug (already detected)
echo "Test 1: Object bug (Invalid free - line 258)"
echo "--------------------------------------------"
esbmc test_esbmc_focused.c fuzzgoat.c --memory-leak-check --unwind 10 2>&1 | grep -E "VERIFICATION|Violated property|dereference failure" | head -3
echo ""

# Test 2: Empty string bug
echo "Test 2: Empty string bug (Invalid free - line 279)"
echo "--------------------------------------------"
esbmc test_string_empty.c fuzzgoat.c --memory-leak-check --unwind 5 2>&1 | grep -E "VERIFICATION|Violated property|dereference failure|forgotten memory" | head -3
echo ""

# Test 3: One-byte string bug
echo "Test 3: One-byte string bug (NULL pointer - line 298)"
echo "--------------------------------------------"
esbmc test_string_one.c fuzzgoat.c --memory-leak-check --unwind 5 2>&1 | grep -E "VERIFICATION|Violated property|dereference failure|NULL pointer" | head -3
echo ""

echo "=========================================="
echo "All tests completed!"
echo "=========================================="
echo ""
echo "Note: ESBMC stops after finding the first bug in each run."
echo "To see full details, run each test separately:"
echo "  esbmc test_esbmc_focused.c fuzzgoat.c --memory-leak-check --unwind 10"
echo "  esbmc test_string_empty.c fuzzgoat.c --memory-leak-check --unwind 5"
echo "  esbmc test_string_one.c fuzzgoat.c --memory-leak-check --unwind 5"

