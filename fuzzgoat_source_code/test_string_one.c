/* Test harness for ESBMC to detect one-byte string bug
 * 
 * This harness creates conditions for ESBMC to explore the one-byte string path.
 * ESBMC will AUTOMATICALLY detect the NULL pointer dereference at line 298.
 */

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
    
    // Set conditions: type = string, length = 1
    // ESBMC will automatically explore this path and detect the bug
    value->type = json_string;
    value->parent = NULL;
    value->u.string.length = 1;
    value->u.string.ptr = (json_char *)malloc(2);
    if (value->u.string.ptr) {
        value->u.string.ptr[0] = 'A';
        value->u.string.ptr[1] = '\0';
    }
    
    // ESBMC will automatically detect NULL pointer dereference at line 298
    json_value_free_ex(&settings, value);
    return 0;
}

