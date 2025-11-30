# HƯỚNG DẪN: PHÁT HIỆN TẤT CẢ CÁC LỖI VỚI ESBMC

## 🔍 VẤN ĐỀ

Khi chạy:
```bash
esbmc test_esbmc_focused.c fuzzgoat.c --memory-leak-check --unwind 10
```

**Chỉ phát hiện được 1 lỗi** (Invalid free - object) vì:

1. **ESBMC dừng khi phát hiện lỗi đầu tiên**: Khi ESBMC tìm thấy lỗi, nó dừng lại và báo `VERIFICATION FAILED`. Nó KHÔNG tiếp tục tìm các lỗi khác.

2. **Test harness tạo type symbolic (0-7)**: ESBMC khám phá các types, nhưng khi phát hiện lỗi ở `type=1` (object), nó dừng lại.

3. **Các lỗi khác cần điều kiện cụ thể**:
   - Empty string bug: cần `type=json_string` VÀ `length=0`
   - One-byte string bug: cần `type=json_string` VÀ `length=1`
   - ESBMC có thể không khám phá đến các điều kiện này trước khi phát hiện lỗi object

## ✅ GIẢI PHÁP

### Cách 1: Chạy nhiều lần với các ràng buộc khác nhau

Tạo các test harness với điều kiện cụ thể để ESBMC tập trung vào từng loại lỗi:

#### Test 1: Phát hiện lỗi Object (đã phát hiện)
```bash
esbmc test_esbmc_focused.c fuzzgoat.c --memory-leak-check --unwind 10
```

#### Test 2: Phát hiện lỗi Empty String
Tạo file `test_string_empty.c`:
```c
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
    
    value->type = json_string;  // ← CHỈ ĐỊNH: type là string
    value->parent = NULL;
    value->u.string.length = 0;  // ← CHỈ ĐỊNH: length = 0 (empty)
    value->u.string.ptr = (json_char *)malloc(1);
    if (value->u.string.ptr) {
        value->u.string.ptr[0] = '\0';
    }
    
    json_value_free_ex(&settings, value);
    return 0;
}
```

Chạy:
```bash
esbmc test_string_empty.c fuzzgoat.c --memory-leak-check --unwind 5
```

#### Test 3: Phát hiện lỗi One-byte String
Tạo file `test_string_one.c`:
```c
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
    
    value->type = json_string;  // ← CHỈ ĐỊNH: type là string
    value->parent = NULL;
    value->u.string.length = 1;  // ← CHỈ ĐỊNH: length = 1
    value->u.string.ptr = (json_char *)malloc(2);
    if (value->u.string.ptr) {
        value->u.string.ptr[0] = 'A';
        value->u.string.ptr[1] = '\0';
    }
    
    json_value_free_ex(&settings, value);
    return 0;
}
```

Chạy:
```bash
esbmc test_string_one.c fuzzgoat.c --memory-leak-check --unwind 5
```

### Cách 2: Sử dụng script tự động

Tạo script `test_all_bugs.sh`:
```bash
#!/bin/bash

echo "=== Test 1: Object bug (đã phát hiện) ==="
esbmc test_esbmc_focused.c fuzzgoat.c --memory-leak-check --unwind 10 2>&1 | grep -E "VERIFICATION|Violated property" | head -5

echo -e "\n=== Test 2: Empty string bug ==="
# Tạo test file động
cat > /tmp/test_string_empty.c << 'EOF'
#include "fuzzgoat.h"
#include <stdlib.h>
static void *wrapper_alloc(size_t s, int z, void *u) { (void)u; return z ? calloc(1,s) : malloc(s); }
static void wrapper_free(void *p, void *u) { (void)u; free(p); }
int main() {
    json_settings s = {0}; s.mem_alloc = wrapper_alloc; s.mem_free = wrapper_free;
    json_value *v = malloc(sizeof(json_value));
    if (!v) return 1;
    v->type = json_string; v->parent = NULL; v->u.string.length = 0;
    v->u.string.ptr = malloc(1); if (v->u.string.ptr) v->u.string.ptr[0] = '\0';
    json_value_free_ex(&s, v);
    return 0;
}
EOF
esbmc /tmp/test_string_empty.c fuzzgoat.c --memory-leak-check --unwind 5 2>&1 | grep -E "VERIFICATION|Violated property" | head -5

echo -e "\n=== Test 3: One-byte string bug ==="
cat > /tmp/test_string_one.c << 'EOF'
#include "fuzzgoat.h"
#include <stdlib.h>
static void *wrapper_alloc(size_t s, int z, void *u) { (void)u; return z ? calloc(1,s) : malloc(s); }
static void wrapper_free(void *p, void *u) { (void)u; free(p); }
int main() {
    json_settings s = {0}; s.mem_alloc = wrapper_alloc; s.mem_free = wrapper_free;
    json_value *v = malloc(sizeof(json_value));
    if (!v) return 1;
    v->type = json_string; v->parent = NULL; v->u.string.length = 1;
    v->u.string.ptr = malloc(2); if (v->u.string.ptr) { v->u.string.ptr[0] = 'A'; v->u.string.ptr[1] = '\0'; }
    json_value_free_ex(&s, v);
    return 0;
}
EOF
esbmc /tmp/test_string_one.c fuzzgoat.c --memory-leak-check --unwind 5 2>&1 | grep -E "VERIFICATION|Violated property" | head -5
```

## 🤔 TẠI SAO LẠI CẦN TẠO NHIỀU TEST HARNESS?

### Hiểu lầm phổ biến:

**"Nếu mục tiêu là tự động phát hiện lỗi, tại sao lại cần tạo test harness cho từng lỗi?"**

### Giải thích:

1. **Test harness KHÔNG phải là "test case cho từng lỗi"**
   - Test harness chỉ định nghĩa **điều kiện** để ESBMC khám phá
   - ESBMC vẫn **tự động** phát hiện lỗi trong điều kiện đó

2. **ESBMC dừng khi phát hiện lỗi đầu tiên**
   - Đây là đặc điểm của Bounded Model Checking
   - Một lần chạy chỉ phát hiện 1 lỗi
   - Để phát hiện nhiều lỗi, cần chạy nhiều lần với điều kiện khác nhau

3. **Ví dụ minh họa:**

```c
// Test harness 1: Chỉ định type = string, length = 0
value->type = json_string;
value->u.string.length = 0;
```

**Đây KHÔNG phải là "test case cho lỗi empty string"!**

**Đây là "điều kiện để ESBMC khám phá":**
- ESBMC vẫn tự động kiểm tra tất cả paths trong điều kiện này
- ESBMC tự động phát hiện lỗi tại dòng 279 (invalid free)
- ESBMC tự động tạo counterexample

**So sánh:**
- ❌ **Test case cụ thể**: "Test với input cụ thể, kiểm tra output cụ thể"
- ✅ **Test harness với symbolic**: "Tạo điều kiện symbolic, ESBMC tự động khám phá và phát hiện lỗi"

## 📊 SO SÁNH CÁC CÁCH TIẾP CẬN

### Cách 1: Test harness tổng quát (test_esbmc_focused.c)

**Ưu điểm:**
- ✅ Một file test tất cả
- ✅ ESBMC tự động khám phá tất cả types

**Nhược điểm:**
- ❌ Chỉ phát hiện 1 lỗi (lỗi đầu tiên)
- ❌ Có thể không khám phá đủ paths

### Cách 2: Test harness riêng cho từng loại lỗi

**Ưu điểm:**
- ✅ Phát hiện được nhiều lỗi
- ✅ Tập trung vào từng loại lỗi
- ✅ Dễ hiểu và debug

**Nhược điểm:**
- ❌ Cần nhiều file test
- ❌ Cần biết trước các loại lỗi (nhưng vẫn không biết chi tiết)

### Cách 3: Sử dụng script tự động

**Ưu điểm:**
- ✅ Tự động chạy tất cả tests
- ✅ Dễ dàng thêm test mới

**Nhược điểm:**
- ❌ Vẫn cần tạo test harness cho từng loại

## 🎯 KẾT LUẬN

**Câu trả lời ngắn gọn:**

1. **ESBMC dừng khi phát hiện lỗi đầu tiên** → Cần chạy nhiều lần
2. **Test harness chỉ định điều kiện khám phá** → Không phải test case cụ thể
3. **Để phát hiện tất cả lỗi** → Chạy với các điều kiện khác nhau

**Lệnh chạy để phát hiện tất cả lỗi:**

```bash
# Lỗi 1: Object (đã phát hiện)
esbmc test_esbmc_focused.c fuzzgoat.c --memory-leak-check --unwind 10

# Lỗi 2: Empty string
esbmc test_string_empty.c fuzzgoat.c --memory-leak-check --unwind 5

# Lỗi 3: One-byte string  
esbmc test_string_one.c fuzzgoat.c --memory-leak-check --unwind 5
```

**Hoặc sử dụng script tự động:**
```bash
chmod +x test_all_bugs.sh
./test_all_bugs.sh
```

