#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "fuzzgoat.h"

// HonggFuzz specific headers
#include <libhfuzz/libhfuzz.h>

// Định nghĩa buffer size tối đa cho input
#define MAX_INPUT_SIZE 1048576  // 1MB

static void process_value(json_value* value, int depth);

static void process_object(json_value* value, int depth)
{
        int length, x;
        if (value == NULL) {
                return;
        }
        length = value->u.object.length;
        for (x = 0; x < length; x++) {
                // Không print để tăng tốc độ
                process_value(value->u.object.values[x].value, depth+1);
        }
}

static void process_array(json_value* value, int depth)
{
        int length, x;
        if (value == NULL) {
                return;
        }
        length = value->u.array.length;
        for (x = 0; x < length; x++) {
                process_value(value->u.array.values[x], depth);
        }
}

static void process_value(json_value* value, int depth)
{
        if (value == NULL) {
                return;
        }
        
        switch (value->type) {
                case json_none:
                        break;
                case json_null:
                        break;
                case json_object:
                        process_object(value, depth+1);
                        break;
                case json_array:
                        process_array(value, depth+1);
                        break;
                case json_integer:
                case json_double:
                case json_string:
                case json_boolean:
                        // Chỉ process, không print
                        break;
        }
}

int main(int argc, char** argv)
{
        json_value* value;
        size_t len;
        const uint8_t* buf;

        // HonggFuzz persistent mode loop
        // HF_ITER cung cấp pointer đến buffer và độ dài
        HF_ITER(&buf, &len);
        
        // Kiểm tra độ dài input hợp lệ
        if (len < 1 || len > MAX_INPUT_SIZE) {
                return 0;
        }
        
        // Parse JSON trực tiếp từ buffer
        value = json_parse((json_char*)buf, len);
        
        if (value != NULL) {
                // Process value để trigger các bug
                process_value(value, 0);
                json_value_free(value);
        }

        return 0;
}
