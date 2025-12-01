# QUY TRÌNH LÀM VIỆC ĐÚNG VỚI ESBMC

## 🎯 NGUYÊN TẮC QUAN TRỌNG

**KHÔNG cần biết trước lỗi là gì!**

- ✅ Tạo test harness với **symbolic values** (chỉ định tối thiểu)
- ✅ Để ESBMC **tự động** khám phá và phát hiện lỗi
- ✅ Chạy → Vá lỗi → Chạy lại → Vá lỗi tiếp theo

## 📋 QUY TRÌNH LÀM VIỆC

### Bước 1: Tạo Test Harness (CHỈ MỘT LẦN)

Tạo file `test_esbmc_auto.c` với:
- **Type symbolic**: `type_choice = nondet_int()` (0-7)
- **Length symbolic**: `arr_len = nondet_int()`, `obj_len = nondet_int()`, `str_len = nondet_int()`
- **Giới hạn hợp lý**: `__ESBMC_assume(... <= 10)` để tránh quá tải

**KHÔNG cần:**
- ❌ Biết trước lỗi là gì
- ❌ Tạo test case cụ thể cho từng lỗi
- ❌ Chỉ định điều kiện cụ thể (như `length = 0`, `length = 1`)

### Bước 2: Chạy ESBMC

```bash
esbmc test_esbmc_auto.c fuzzgoat.c --memory-leak-check --unwind 10
```

**Kết quả:**
- ESBMC tự động khám phá TẤT CẢ paths
- Phát hiện lỗi đầu tiên (nếu có)
- Báo `VERIFICATION FAILED` với counterexample

### Bước 3: Vá lỗi

Dựa vào counterexample từ ESBMC:
```
Violated property:
  file fuzzgoat.c line 258
  dereference failure: array bounds violated

Counterexample:
  type_choice = 1 (json_object)
  obj_len = 4
```

→ Vá lỗi tại dòng 258 trong `fuzzgoat.c`

### Bước 4: Chạy lại ESBMC

```bash
esbmc test_esbmc_auto.c fuzzgoat.c --memory-leak-check --unwind 10
```

**Kết quả:**
- Lỗi đầu tiên đã được vá → ESBMC tiếp tục khám phá
- Phát hiện lỗi tiếp theo (nếu có)
- Báo `VERIFICATION FAILED` với counterexample mới

### Bước 5: Lặp lại

Lặp lại Bước 3-4 cho đến khi:
- ESBMC báo `VERIFICATION SUCCESSFUL`
- Hoặc không còn lỗi nào

## 🔍 TẠI SAO CÁCH NÀY ĐÚNG?

### 1. Test Harness chỉ định TỐI THIỂU

```c
// CHỈ ĐỊNH: Type có thể là 0-7 (tất cả json_type)
int type_choice = nondet_int();
__ESBMC_assume(type_choice >= 0 && type_choice <= 7);

// CHỈ ĐỊNH: Length có thể là 0-10 (giới hạn để tránh quá tải)
unsigned int obj_len = nondet_int();
__ESBMC_assume(obj_len <= 10);
```

**KHÔNG chỉ định:**
- ❌ `length = 0` (cụ thể)
- ❌ `length = 1` (cụ thể)
- ❌ `type = json_string` (cụ thể)

### 2. ESBMC tự động khám phá

ESBMC sẽ tự động:
- Khám phá TẤT CẢ các paths: `type = 0, 1, 2, ..., 7`
- Khám phá TẤT CẢ các sub-paths: `length = 0, 1, 2, ..., 10`
- Phát hiện lỗi ở BẤT KỲ path nào có lỗi

### 3. ESBMC dừng khi phát hiện lỗi đầu tiên

**Đây là đặc điểm của Bounded Model Checking:**
- Mục tiêu: Tìm xem có lỗi KHÔNG (có/không)
- Khi tìm thấy lỗi: Dừng lại và báo lỗi
- **KHÔNG phải là hạn chế** - đây là cách BMC hoạt động

**Giải pháp:** Vá lỗi và chạy lại → ESBMC sẽ phát hiện lỗi tiếp theo

## 📊 SO SÁNH CÁC CÁCH TIẾP CẬN

### ❌ Cách SAI: Tạo test case cụ thể cho từng lỗi

```c
// test_string_empty.c
value->type = json_string;  // ← Cụ thể!
value->u.string.length = 0;  // ← Cụ thể!
```

**Vấn đề:**
- Phải biết trước lỗi là gì
- Phải tạo nhiều file test
- Không còn là "tự động phát hiện"

### ✅ Cách ĐÚNG: Tạo symbolic và để ESBMC tự động

```c
// test_esbmc_auto.c
int type_choice = nondet_int();  // ← Symbolic!
__ESBMC_assume(type_choice >= 0 && type_choice <= 7);
value->type = (json_type)type_choice;

unsigned int str_len = nondet_int();  // ← Symbolic!
__ESBMC_assume(str_len <= 10);
value->u.string.length = str_len;
```

**Ưu điểm:**
- ✅ KHÔNG cần biết trước lỗi
- ✅ Chỉ một file test
- ✅ ESBMC tự động phát hiện

## 🎯 VÍ DỤ THỰC TẾ

### Lần chạy 1:

```bash
$ esbmc test_esbmc_auto.c fuzzgoat.c --memory-leak-check --unwind 10
VERIFICATION FAILED
  file fuzzgoat.c line 258
  dereference failure: array bounds violated
  Counterexample: type_choice = 1, obj_len = 4
```

**Hành động:** Vá lỗi tại dòng 258

### Lần chạy 2:

```bash
$ esbmc test_esbmc_auto.c fuzzgoat.c --memory-leak-check --unwind 10
VERIFICATION FAILED
  file fuzzgoat.c line 279
  dereference failure: forgotten memory
  Counterexample: type_choice = 5, str_len = 0
```

**Hành động:** Vá lỗi tại dòng 279

### Lần chạy 3:

```bash
$ esbmc test_esbmc_auto.c fuzzgoat.c --memory-leak-check --unwind 10
VERIFICATION FAILED
  file fuzzgoat.c line 298
  dereference failure: NULL pointer
  Counterexample: type_choice = 5, str_len = 1
```

**Hành động:** Vá lỗi tại dòng 298

### Lần chạy 4:

```bash
$ esbmc test_esbmc_auto.c fuzzgoat.c --memory-leak-check --unwind 10
VERIFICATION SUCCESSFUL
```

**Kết quả:** Không còn lỗi!

## 💡 TẠI SAO CẦN GIỚI HẠN (assume)?

### Vấn đề nếu không giới hạn:

```c
unsigned int obj_len = nondet_int();  // Có thể là: 0, 1, 2, ..., 1000000, ...
// Không có assume!
```

**Hậu quả:**
- ESBMC khám phá quá nhiều paths
- Tốn thời gian và bộ nhớ
- Có thể không bao giờ kết thúc

### Giải pháp: Giới hạn hợp lý

```c
unsigned int obj_len = nondet_int();
__ESBMC_assume(obj_len <= 10);  // Giới hạn: 0-10
```

**Lý do:**
- ✅ Đủ để phát hiện lỗi (lỗi thường xảy ra ở giá trị nhỏ: 0, 1, 4, ...)
- ✅ Không quá tải hiệu năng
- ✅ ESBMC vẫn tự động khám phá (không phải chỉ định cụ thể)

## 📝 TÓM TẮT

### Nguyên tắc:

1. **Chỉ định TỐI THIỂU**: Chỉ type và length là symbolic
2. **Để ESBMC tự động**: Không chỉ định điều kiện cụ thể
3. **Quy trình lặp**: Chạy → Vá → Chạy lại

### Test Harness đúng:

```c
// CHỈ ĐỊNH: Type symbolic (0-7)
int type_choice = nondet_int();
__ESBMC_assume(type_choice >= 0 && type_choice <= 7);

// CHỈ ĐỊNH: Length symbolic (0-10)
unsigned int obj_len = nondet_int();
__ESBMC_assume(obj_len <= 10);

// ESBMC TỰ ĐỘNG khám phá TẤT CẢ paths và phát hiện lỗi
```

### KHÔNG cần:

- ❌ Biết trước lỗi là gì
- ❌ Tạo test case cụ thể
- ❌ Chỉ định `length = 0`, `length = 1`, v.v.

### CẦN:

- ✅ Tạo symbolic values
- ✅ Giới hạn hợp lý (để tránh quá tải)
- ✅ Vá lỗi và chạy lại (vì ESBMC dừng khi phát hiện lỗi đầu tiên)

## 🎉 KẾT LUẬN

**Cách làm đúng:**
1. Tạo một test harness với symbolic values (chỉ định tối thiểu)
2. Chạy ESBMC → Phát hiện lỗi đầu tiên
3. Vá lỗi → Chạy lại → Phát hiện lỗi tiếp theo
4. Lặp lại cho đến khi không còn lỗi

**Đây chính là cách sử dụng ESBMC đúng đắn!**

