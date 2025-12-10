#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "fuzzgoat.h"

// AFL++ persistent mode - chạy nhiều test case trong 1 process
#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_FUZZ_INIT();
#endif

// Định nghĩa buffer size tối đa cho input
#define MAX_INPUT_SIZE 1048576  // 1MB

static void print_depth_shift(int depth)
{
        int j;
        for (j=0; j < depth; j++) {
                printf(" ");
        }
}

static void process_value(json_value* value, int depth);

static void process_object(json_value* value, int depth)
{
        int length, x;
        if (value == NULL) {
                return;
        }
        length = value->u.object.length;
        for (x = 0; x < length; x++) {
                print_depth_shift(depth);
                printf("object[%d].name = %s\n", x, value->u.object.values[x].name);
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
        printf("array\n");
        for (x = 0; x < length; x++) {
                process_value(value->u.array.values[x], depth);
        }
}

static void process_value(json_value* value, int depth)
{
        int j;
        if (value == NULL) {
                return;
        }
        if (value->type != json_object) {
                print_depth_shift(depth);
        }
        switch (value->type) {
                case json_none:
                        printf("none\n");
                        break;
                case json_object:
                        process_object(value, depth+1);
                        break;
                case json_array:
                        process_array(value, depth+1);
                        break;
                case json_integer:
                        printf("int: %10" PRId64 "\n", value->u.integer);
                        break;
                case json_double:
                        printf("double: %f\n", value->u.dbl);
                        break;
                case json_string:
                        printf("string: %s\n", value->u.string.ptr);
                        break;
                case json_boolean:
                        printf("bool: %d\n", value->u.boolean);
                        break;
        }
}

int main(int argc, char** argv)
{
        // Khởi tạo buffer tĩnh để tái sử dụng trong persistent mode
        static unsigned char input_buf[MAX_INPUT_SIZE];
        ssize_t len;
        json_value* value;

        // Persistent mode: AFL++ sẽ gọi hàm này nhiều lần
        // Deferred initialization: trì hoãn fork server đến sau khi khởi tạo
        __AFL_INIT();
        
        unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;  // Shared memory buffer
        
        while (__AFL_LOOP(10000)) {  // Chạy 10000 iterations mỗi fork
                len = __AFL_FUZZ_TESTCASE_LEN;  // Lấy độ dài input
                
                if (len < 1 || len > MAX_INPUT_SIZE) continue;
                
                // Copy từ shared memory vào buffer của chúng ta
                memcpy(input_buf, buf, len);
                input_buf[len] = '\0';
                
                // Parse JSON
                value = json_parse((json_char*)input_buf, len);
                
                if (value != NULL) {
                        // Process value để trigger các bug
                        process_value(value, 0);
                        json_value_free(value);
                }
        }

        return 0;
}
