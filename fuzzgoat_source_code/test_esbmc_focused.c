/* Test harness for ESBMC to detect vulnerabilities in json_value_free_ex
 * 
 * This harness creates json_value structures with symbolic but bounded values
 * to let ESBMC discover the 4 vulnerabilities.
 */

#include "fuzzgoat.h"
#include <stdlib.h>
#include <string.h>

// ESBMC built-in functions
extern int nondet_int();

// Wrapper functions to match json_settings signature
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
    
    // Create a symbolic json_value
    json_value *value = (json_value *)malloc(sizeof(json_value));
    if (!value) {
        return 1;
    }
    
    // Make the type symbolic - ESBMC will explore all possible types
    int type_choice = nondet_int();
    __ESBMC_assume(type_choice >= 0 && type_choice <= 7);
    
    value->type = (json_type)type_choice;
    value->parent = NULL;
    
    // For each type, set up the structure with bounded symbolic values
    switch (value->type) {
        case json_array: {
            // Symbolic array length - limit to small values to avoid long loops
            unsigned int arr_len = nondet_int();
            __ESBMC_assume(arr_len <= 5);  // Limit to 5 to avoid excessive unwinding
            value->u.array.length = arr_len;
            
            if (arr_len > 0) {
                value->u.array.values = (json_value **)malloc(arr_len * sizeof(json_value *));
                if (!value->u.array.values) {
                    free(value);
                    return 1;
                }
                // Initialize with NULL - ESBMC will explore this
                int i;
                for (i = 0; i < arr_len; i++) {
                    value->u.array.values[i] = NULL;
                }
            } else {
                value->u.array.values = NULL;
            }
            break;
        }
        
        case json_object: {
            // Symbolic object length - limit to small values
            unsigned int obj_len = nondet_int();
            __ESBMC_assume(obj_len <= 5);
            value->u.object.length = obj_len;
            
            if (obj_len > 0) {
                value->u.object.values = (json_object_entry *)malloc(obj_len * sizeof(json_object_entry));
                if (!value->u.object.values) {
                    free(value);
                    return 1;
                }
                // Initialize entries
                int i;
                for (i = 0; i < obj_len; i++) {
                    value->u.object.values[i].name = NULL;
                    value->u.object.values[i].name_length = 0;
                    value->u.object.values[i].value = NULL;
                }
            } else {
                value->u.object.values = NULL;
            }
            break;
        }
        
        case json_string: {
            // Symbolic string length - limit to small values (0, 1, 2, etc.)
            unsigned int str_len = nondet_int();
            __ESBMC_assume(str_len <= 5);
            value->u.string.length = str_len;
            
            if (str_len > 0) {
                value->u.string.ptr = (json_char *)malloc((str_len + 1) * sizeof(json_char));
                if (!value->u.string.ptr) {
                    free(value);
                    return 1;
                }
                // Initialize string - ESBMC will explore this
                int i;
                for (i = 0; i < str_len; i++) {
                    value->u.string.ptr[i] = 'A';
                }
                value->u.string.ptr[str_len] = '\0';
            } else {
                value->u.string.ptr = NULL;
            }
            break;
        }
        
        default:
            // For other types, just initialize to 0
            memset(&value->u, 0, sizeof(value->u));
            break;
    }
    
    // Call json_value_free_ex - ESBMC will detect the 4 vulnerabilities:
    // 1. Use after free (empty array) - when type is json_array and length is 0
    // 2. Invalid free (object) - when type is json_object (bug in length decrement)
    // 3. Invalid free (empty string) - when type is json_string and length is 0
    // 4. NULL pointer dereference (one-byte string) - when type is json_string and length is 1
    json_value_free_ex(&settings, value);
    
    return 0;
}

