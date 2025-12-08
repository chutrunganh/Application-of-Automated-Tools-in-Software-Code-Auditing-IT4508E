/* Test harness for ESBMC to detect Bug 3: Invalid Pointer Free (empty string)
 * 
 * Bug 3: In json_value_free_ex, when string length is 0, the code decrements
 * the pointer before freeing it, causing an invalid free.
 * 
 * The issue with the original harness: 
 * 1. It allocated ptr with malloc(1), but ESBMC may not track that ptr-- 
 *    makes it point outside the allocated region
 * 2. We need to make the length symbolic so ESBMC explores both paths
 * 3. We need to ensure ESBMC understands the pointer arithmetic relationship
 */

#include "fuzzgoat.h"
#include <stdlib.h>
#include <string.h>

// ESBMC built-in functions
extern int nondet_int();

// Track the original allocated pointer to help ESBMC detect invalid free
static void *tracked_allocated_ptr = NULL;
static size_t tracked_allocated_size = 0;

// Wrapper functions to match json_settings signature
static void *wrapper_alloc(size_t size, int zero, void *user_data) {
    (void)user_data;
    void *ptr = zero ? calloc(1, size) : malloc(size);
    if (ptr) {
        tracked_allocated_ptr = ptr;
        tracked_allocated_size = size;
    }
    return ptr;
}

static void wrapper_free(void *ptr, void *user_data) {
    (void)user_data;
    // For bug 3: when length == 0, ptr is decremented before free
    // After ptr--, ptr points to (tracked_allocated_ptr - 1)
    // This is outside the allocated region, so free() should fail
    // ESBMC should detect this as an invalid pointer free
    
    // Add assertion to help ESBMC understand the relationship
    // If ptr was decremented, it should not equal the original allocated pointer
    if (tracked_allocated_ptr != NULL) {
        // After ptr--, ptr should be less than tracked_allocated_ptr
        // This assertion will fail if ptr was decremented
        __ESBMC_assert(ptr >= tracked_allocated_ptr, 
                      "Pointer should be within allocated region");
    }
    
    free(ptr);
}

int main() {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    // Create a json_value
    json_value *value = (json_value *)malloc(sizeof(json_value));
    if (!value) {
        return 1;
    }
    
    value->type = json_string;
    value->parent = NULL;
    
    // Focus on empty string (length = 0) for bug 3
    // Bug 3: when length == 0, code does ptr-- before free(ptr)
    unsigned int str_len = nondet_int();
    __ESBMC_assume(str_len == 0);  // Focus specifically on empty string for bug 3
    value->u.string.length = str_len;
    
    // Allocate memory: when length = 0, we allocate 1 byte (for null terminator)
    // This simulates what happens in new_value() when parsing a string
    size_t alloc_size = (str_len + 1) * sizeof(json_char);
    value->u.string.ptr = (json_char *)settings.mem_alloc(alloc_size, 0, settings.user_data);
    
    if (!value->u.string.ptr) {
        free(value);
        return 1;
    }
    
    // Initialize the string based on length
    if (str_len > 0) {
        int i;
        for (i = 0; i < str_len; i++) {
            value->u.string.ptr[i] = 'A';
        }
    }
    value->u.string.ptr[str_len] = '\0';  // Null terminator
    
    // Now call json_value_free_ex
    // When length == 0:
    //   1. Code does: value->u.string.ptr--  (decrements pointer)
    //   2. Then calls: mem_free(value->u.string.ptr, ...)
    //   3. This free() is called on a pointer that is 1 byte before the allocated region
    //   4. ESBMC should detect this as an invalid free
    json_value_free_ex(&settings, value);
    
    return 0;
}

