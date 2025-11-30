#!/bin/bash
# Script to run ESBMC tests for different vulnerability scenarios

cd "$(dirname "$0")"

echo "=== Running ESBMC tests to detect vulnerabilities in fuzzgoat.c ==="
echo ""

# Test 1: Focused test (already found object bug)
echo "Test 1: Focused test (all types with bounded values)"
timeout 600 esbmc test_esbmc_focused.c fuzzgoat.c --memory-leak-check --unwind 10 2>&1 | tee esbmc_test1.txt
echo ""

# Test 2: Test with specific array length = 0
echo "Test 2: Testing empty array scenario"
cat > test_array_empty.c << 'EOF'
#include "fuzzgoat.h"
#include <stdlib.h>

static void *wrapper_alloc(size_t size, int zero, void *user_data) {
    (void)user_data;
    return zero ? calloc(1, size) : malloc(size);
}

static void wrapper_free(void *ptr, void *user_data) {
    (void)user_data;
    free(ptr);
}

int main() {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    json_value *value = (json_value *)malloc(sizeof(json_value));
    if (!value) return 1;
    
    value->type = json_array;
    value->parent = NULL;
    value->u.array.length = 0;
    value->u.array.values = NULL;
    
    json_value_free_ex(&settings, value);
    return 0;
}
EOF
timeout 300 esbmc test_array_empty.c fuzzgoat.c --memory-leak-check --unwind 5 2>&1 | tee esbmc_test2.txt
echo ""

# Test 3: Test with string length = 0
echo "Test 3: Testing empty string scenario"
cat > test_string_empty.c << 'EOF'
#include "fuzzgoat.h"
#include <stdlib.h>

static void *wrapper_alloc(size_t size, int zero, void *user_data) {
    (void)user_data;
    return zero ? calloc(1, size) : malloc(size);
}

static void wrapper_free(void *ptr, void *user_data) {
    (void)user_data;
    free(ptr);
}

int main() {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    json_value *value = (json_value *)malloc(sizeof(json_value));
    if (!value) return 1;
    
    value->type = json_string;
    value->parent = NULL;
    value->u.string.length = 0;
    value->u.string.ptr = (json_char *)malloc(1);
    if (value->u.string.ptr) {
        value->u.string.ptr[0] = '\0';
    }
    
    json_value_free_ex(&settings, value);
    return 0;
}
EOF
timeout 300 esbmc test_string_empty.c fuzzgoat.c --memory-leak-check --unwind 5 2>&1 | tee esbmc_test3.txt
echo ""

# Test 4: Test with string length = 1
echo "Test 4: Testing one-byte string scenario"
cat > test_string_one.c << 'EOF'
#include "fuzzgoat.h"
#include <stdlib.h>

static void *wrapper_alloc(size_t size, int zero, void *user_data) {
    (void)user_data;
    return zero ? calloc(1, size) : malloc(size);
}

static void wrapper_free(void *ptr, void *user_data) {
    (void)user_data;
    free(ptr);
}

int main() {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    json_value *value = (json_value *)malloc(sizeof(json_value));
    if (!value) return 1;
    
    value->type = json_string;
    value->parent = NULL;
    value->u.string.length = 1;
    value->u.string.ptr = (json_char *)malloc(2);
    if (value->u.string.ptr) {
        value->u.string.ptr[0] = 'A';
        value->u.string.ptr[1] = '\0';
    }
    
    json_value_free_ex(&settings, value);
    return 0;
}
EOF
timeout 300 esbmc test_string_one.c fuzzgoat.c --memory-leak-check --unwind 5 2>&1 | tee esbmc_test4.txt
echo ""

echo "=== All tests completed ==="

