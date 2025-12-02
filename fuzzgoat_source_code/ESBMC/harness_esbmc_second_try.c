/* Harness to drive parser into new_value() json_array empty branch (UAF)
 * This uses json_parse("[]") so the parser builds an empty array and
 * hits the vulnerable free(*top) path on the second pass.
 */

#include "fuzzgoat.h"
#include <stdlib.h>
#include <string.h>

static void *wrapper_alloc(size_t size, int zero, void *user_data) {
    (void)user_data;
    return zero ? calloc(1, size) : malloc(size);
}

static void wrapper_free(void *ptr, void *user_data) {
    (void)user_data;
    free(ptr);
}

int main(void) {
    // Minimal settings with custom alloc/free to make ESBMC track memory
    json_settings settings;
    memset(&settings, 0, sizeof(settings));
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;

    const json_char input[] = "[]"; // empty array payload

    // Use json_parse_ex to pass in our settings explicitly
    char errbuf[128];
    json_value *v = json_parse_ex(&settings, input, sizeof(input) - 1, errbuf);

    // If parsing returns a value, free it to complete the typical lifecycle
    // (the UAF vulnerability is inside parsing passes, not here)
    if (v) {
        json_value_free_ex(&settings, v);
    }

    return 0;
}
