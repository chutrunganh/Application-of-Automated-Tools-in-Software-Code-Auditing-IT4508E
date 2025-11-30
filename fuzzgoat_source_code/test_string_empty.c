/* Test harness for ESBMC to detect empty string bug
 * 
 * This harness creates conditions for ESBMC to explore the empty string path.
 * ESBMC will AUTOMATICALLY detect the bug at line 279.
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
    
    // Set conditions: type = string, length = 0
    // ESBMC will automatically explore this path and detect the bug
    value->type = json_string;
    value->parent = NULL;
    value->u.string.length = 0;
    value->u.string.ptr = (json_char *)malloc(1);
    if (value->u.string.ptr) {
        value->u.string.ptr[0] = '\0';
    }
    
    // ESBMC will automatically detect invalid free at line 279
    json_value_free_ex(&settings, value);
    return 0;
}

