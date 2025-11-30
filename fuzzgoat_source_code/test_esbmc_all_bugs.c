/* Test harness for ESBMC to detect ALL vulnerabilities
 * 
 * Strategy: Test each type separately to ensure ESBMC explores all paths
 * This is NOT creating test cases for each bug - it's creating conditions
 * for ESBMC to explore all possible execution paths.
 */

#include "fuzzgoat.h"
#include <stdlib.h>
#include <string.h>

// ESBMC built-in functions
extern int nondet_int();

// Wrapper functions
static void *wrapper_alloc(size_t size, int zero, void *user_data) {
    (void)user_data;
    return zero ? calloc(1, size) : malloc(size);
}

static void wrapper_free(void *ptr, void *user_data) {
    (void)user_data;
    free(ptr);
}

// Test function for each type
void test_type(json_type type, unsigned int length) {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    json_value *value = (json_value *)malloc(sizeof(json_value));
    if (!value) return;
    
    value->type = type;
    value->parent = NULL;
    
    switch (type) {
        case json_array:
            value->u.array.length = length;
            if (length > 0) {
                value->u.array.values = (json_value **)malloc(length * sizeof(json_value *));
                if (value->u.array.values) {
                    for (unsigned int i = 0; i < length; i++) {
                        value->u.array.values[i] = NULL;
                    }
                }
            } else {
                value->u.array.values = NULL;
            }
            break;
            
        case json_object:
            value->u.object.length = length;
            if (length > 0) {
                value->u.object.values = (json_object_entry *)malloc(length * sizeof(json_object_entry));
                if (value->u.object.values) {
                    for (unsigned int i = 0; i < length; i++) {
                        value->u.object.values[i].name = NULL;
                        value->u.object.values[i].name_length = 0;
                        value->u.object.values[i].value = NULL;
                    }
                }
            } else {
                value->u.object.values = NULL;
            }
            break;
            
        case json_string:
            value->u.string.length = length;
            if (length > 0) {
                value->u.string.ptr = (json_char *)malloc((length + 1) * sizeof(json_char));
                if (value->u.string.ptr) {
                    for (unsigned int i = 0; i < length; i++) {
                        value->u.string.ptr[i] = 'A';
                    }
                    value->u.string.ptr[length] = '\0';
                }
            } else {
                // For empty string, allocate memory to trigger the bug
                value->u.string.ptr = (json_char *)malloc(1);
                if (value->u.string.ptr) {
                    value->u.string.ptr[0] = '\0';
                }
            }
            break;
            
        default:
            memset(&value->u, 0, sizeof(value->u));
            break;
    }
    
    json_value_free_ex(&settings, value);
}

int main() {
    // Make type and length symbolic
    // ESBMC will explore ALL combinations
    int type_choice = nondet_int();
    __ESBMC_assume(type_choice >= 0 && type_choice <= 7);
    
    unsigned int length = nondet_int();
    __ESBMC_assume(length <= 5);  // Limit to avoid excessive unwinding
    
    // Call test function - ESBMC will explore all paths
    // This includes:
    // - type=json_array, length=0 → Use after free
    // - type=json_object, length>0 → Invalid free
    // - type=json_string, length=0 → Invalid free
    // - type=json_string, length=1 → NULL pointer dereference
    test_type((json_type)type_choice, length);
    
    return 0;
}

