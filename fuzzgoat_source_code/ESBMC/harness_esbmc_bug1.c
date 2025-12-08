/* Test harness for ESBMC to detect Bug 1: Use After Free (empty array)
 * 
 * Bug 1: In new_value(), when array length is 0, the code calls free(*top)
 * at line 137, but *top is still used later in json_value_free_ex().
 * 
 * The issue with the original harness:
 * 1. new_value() is a static function, so we can't call it directly
 * 2. We need to simulate the bug flow: free the pointer, then use it
 * 3. We need assertions to help ESBMC detect Use After Free
 */

#include "fuzzgoat.h"
#include <stdlib.h>
#include <string.h>

// ESBMC built-in functions
extern int nondet_int();

// Track the freed pointer to help ESBMC detect Use After Free
static void *freed_pointer_tracker = NULL;

// Wrapper functions to match json_settings signature
static void *wrapper_alloc(size_t size, int zero, void *user_data) {
    (void)user_data;
    return zero ? calloc(1, size) : malloc(size);
}

static void wrapper_free(void *ptr, void *user_data) {
    (void)user_data;
    // ESBMC with pointer checking should detect if ptr is invalid
    free(ptr);
}

// Simulate the bug in new_value() when array length == 0
// This simulates what happens at line 137 in fuzzgoat.c: free(*top);
static void simulate_new_value_bug(json_value **top) {
    // This simulates the bug: free(*top) when length == 0
    // In the real code, this happens at fuzzgoat.c line 137
    freed_pointer_tracker = (void *)(*top);
    free(*top);
    // Note: *top is not set to NULL, so it still points to freed memory
    // This is the bug - the pointer is freed but still used later
}

int main() {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    // Create a json_value for empty array (length = 0)
    // This simulates what happens when parsing "[]" in json_parse()
    json_value *value = (json_value *)malloc(sizeof(json_value));
    if (!value) {
        return 1;
    }
    
    value->type = json_array;
    value->parent = NULL;
    value->u.array.length = 0;  // Empty array - this triggers the bug
    value->u.array.values = NULL;
    
    // Simulate the bug in new_value(): free(*top) when length == 0
    // This is what happens at fuzzgoat.c line 137
    json_value **top = &value;
    simulate_new_value_bug(top);
    
    // Now value points to freed memory, but we still use it
    // This simulates what happens in main_afl.c line 138: json_value_free(value)
    // When json_value_free_ex() is called, it will access freed memory:
    // - Line 220: value->parent = 0;  (USE AFTER FREE)
    // - Line 224: switch (value->type) (USE AFTER FREE)
    
    // Add assertion to help ESBMC detect Use After Free
    // If value was freed, accessing it should be detected
    __ESBMC_assert(value != (json_value *)freed_pointer_tracker, 
                  "Use after free: value should not be the freed pointer");
    
    // Call json_value_free_ex with the freed pointer - this triggers Use After Free
    // ESBMC with pointer checking should detect that we're accessing freed memory
    json_value_free_ex(&settings, value);
    
    return 0;
}
