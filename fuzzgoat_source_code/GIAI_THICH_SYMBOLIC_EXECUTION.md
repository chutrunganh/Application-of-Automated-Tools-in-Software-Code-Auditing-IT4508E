# GIẢI THÍCH: TẠI SAO CẦN TEST HARNESS VỚI SYMBOLIC EXECUTION?

## 1. HIỂU LẦM PHỔ BIẾN

Nhiều người nghĩ rằng với **Symbolic Execution**, tool sẽ:
- ✅ Tự động nhận diện tất cả input
- ✅ Tự động biết đâu là điểm vào
- ✅ Tự động tạo symbolic values cho mọi biến

**NHƯNG THỰC TẾ KHÔNG PHẢI VẬY!**

## 2. VẤN ĐỀ VỚI FUZZGOAT.C

### 2.1. Nếu chỉ có fuzzgoat.c, ESBMC sẽ gặp vấn đề gì?

```c
// fuzzgoat.c chỉ chứa các hàm:
json_value * json_parse(...) { ... }
void json_value_free_ex(...) { ... }
```

**ESBMC không biết:**
- ❌ Hàm nào cần được gọi? (`json_parse` hay `json_value_free_ex`?)
- ❌ Gọi với tham số gì?
- ❌ Cấu trúc `json_value` nên được tạo như thế nào?
- ❌ Đâu là input từ bên ngoài (cần symbolic)?
- ❌ Đâu là biến nội bộ (có thể concrete)?

### 2.2. Ví dụ minh họa

Giả sử bạn có hàm này:

```c
int add(int a, int b) {
    return a + b;
}
```

**Câu hỏi:** ESBMC nên test với `a` và `b` bằng bao nhiêu?

- Nếu `a = 5, b = 3` → concrete execution (chạy 1 lần)
- Nếu `a = symbolic, b = symbolic` → symbolic execution (khám phá tất cả giá trị)

**ESBMC cần bạn chỉ định:** "Hãy làm cho `a` và `b` là symbolic!"

## 3. TEST HARNESS LÀ GÌ VÀ TẠI SAO CẦN?

### 3.1. Test Harness = "Hướng dẫn" cho ESBMC

Test harness là code C đơn giản để:
1. **Định nghĩa điểm vào**: Hàm `main()` - nơi bắt đầu
2. **Tạo symbolic values**: Sử dụng `nondet_int()`, `nondet_char()`
3. **Setup môi trường**: Khởi tạo cấu trúc dữ liệu, function pointers
4. **Gọi hàm cần test**: Invoke các hàm trong code cần kiểm thử

### 3.2. So sánh: Có vs Không có Test Harness

#### ❌ KHÔNG có test harness:

```bash
esbmc fuzzgoat.c
```

**Kết quả:**
- ESBMC không biết bắt đầu từ đâu
- Không có hàm `main()` để chạy
- Không biết test cái gì
- → **LỖI: No main function**

#### ✅ CÓ test harness:

```c
// test_harness.c
int main() {
    int a = nondet_int();  // ESBMC: "a là symbolic!"
    int b = nondet_int();   // ESBMC: "b là symbolic!"
    int result = add(a, b); // ESBMC: "Test hàm add() với a, b symbolic"
    assert(result == a + b);
}
```

```bash
esbmc test_harness.c fuzzgoat.c
```

**Kết quả:**
- ESBMC biết bắt đầu từ `main()`
- Biết `a` và `b` là symbolic
- Khám phá tất cả giá trị có thể của `a` và `b`
- Kiểm tra assertion

## 4. VÍ DỤ CỤ THỂ VỚI FUZZGOAT

### 4.1. Test Harness chúng ta đã tạo

```c
int main() {
    // 1. Tạo json_value với type SYMBOLIC
    int type_choice = nondet_int();  // ← ĐÂY! Chỉ định "type là symbolic"
    __ESBMC_assume(type_choice >= 0 && type_choice <= 7);
    
    value->type = (json_type)type_choice;
    
    // 2. Tạo length SYMBOLIC
    unsigned int arr_len = nondet_int();  // ← ĐÂY! Chỉ định "length là symbolic"
    __ESBMC_assume(arr_len <= 5);
    value->u.array.length = arr_len;
    
    // 3. Gọi hàm cần test
    json_value_free_ex(&settings, value);  // ← ESBMC test hàm này
}
```

### 4.2. ESBMC làm gì với test harness này?

1. **Nhận diện symbolic values:**
   - `type_choice = nondet_int()` → ESBMC: "OK, type có thể là 0, 1, 2, ..., 7"
   - `arr_len = nondet_int()` → ESBMC: "OK, length có thể là 0, 1, 2, ..., 5"

2. **Khám phá tất cả paths:**
   ```
   type = 0 (json_none)     → Test path 1
   type = 1 (json_object)   → Test path 2
   type = 2 (json_array)    → Test path 3
   ...
   ```

3. **Với mỗi path, khám phá sub-paths:**
   ```
   type = 2, length = 0 → Test empty array
   type = 2, length = 1 → Test array with 1 element
   type = 2, length = 2 → Test array with 2 elements
   ...
   ```

4. **Phát hiện lỗi:**
   - Khi `type = 1, length = 4` → Phát hiện array bounds violation tại dòng 258!

## 5. TẠI SAO KHÔNG TỰ ĐỘNG?

### 5.1. Vấn đề với "Tự động nhận diện"

Nếu ESBMC tự động làm mọi thứ symbolic:

```c
void process_data(int x) {
    int y = x + 1;  // Nếu y cũng symbolic → quá nhiều paths!
    int z = y * 2;  // Nếu z cũng symbolic → state explosion!
    // ...
}
```

**Vấn đề:**
- **State explosion**: Quá nhiều paths cần khám phá
- **Không hiệu quả**: Mất quá nhiều thời gian
- **Không chính xác**: Không biết đâu là input thực sự

### 5.2. Giải pháp: Test Harness chỉ định rõ ràng

```c
int main() {
    int x = nondet_int();  // ← CHỈ x là symbolic (input)
    process_data(x);       // ← y, z là internal (tính từ x)
}
```

**Lợi ích:**
- ✅ Rõ ràng: `x` là input, `y`, `z` là derived
- ✅ Hiệu quả: Chỉ khám phá paths từ input
- ✅ Kiểm soát: Developer quyết định đâu là symbolic

## 6. SO SÁNH VỚI CÁC TOOL KHÁC

### 6.1. AFL (Fuzzing tool)

```bash
afl-fuzz -i input_dir -o output_dir ./program @@
```

- AFL tự động tạo input files
- Nhưng vẫn cần chỉ định: input directory, output directory, program
- **Tương tự**: Cần "hướng dẫn" (command line args)

### 6.2. Static Analysis Tools (như Clang Static Analyzer)

```bash
scan-build make
```

- Tự động phân tích toàn bộ codebase
- **NHƯNG**: Chỉ phát hiện lỗi trong phạm vi một hàm
- **KHÔNG**: Phát hiện lỗi cần nhiều hàm tương tác (như use-after-free)

### 6.3. ESBMC (Symbolic Execution)

```bash
esbmc test_harness.c source.c
```

- Cần test harness để chỉ định:
  - Entry point (`main()`)
  - Symbolic inputs (`nondet_*()`)
  - Test scenario (gọi hàm nào, với gì)
- **NHƯNG**: Phát hiện lỗi phức tạp, cross-function

## 7. KẾT LUẬN

### 7.1. Tại sao cần test harness?

1. **Định nghĩa input**: Chỉ định biến nào là symbolic
2. **Setup môi trường**: Khởi tạo cấu trúc dữ liệu phức tạp
3. **Điểm vào**: Cung cấp hàm `main()` để ESBMC bắt đầu
4. **Kiểm soát**: Giới hạn không gian tìm kiếm (tránh state explosion)

### 7.2. Symbolic Execution ≠ Tự động hoàn toàn

- ✅ **Tự động**: Khám phá tất cả paths từ symbolic inputs
- ✅ **Tự động**: Tạo test cases (counterexamples)
- ❌ **KHÔNG tự động**: Nhận diện đâu là input
- ❌ **KHÔNG tự động**: Biết cách gọi hàm
- ❌ **KHÔNG tự động**: Setup môi trường

### 7.3. Test Harness = "Cầu nối"

Test harness là cầu nối giữa:
- **Code cần test** (fuzzgoat.c) - không có main, không biết input
- **ESBMC** - cần biết entry point và symbolic inputs

**Giống như:**
- Code = Nhà máy (có máy móc nhưng không biết nguyên liệu)
- Test Harness = Người vận hành (đưa nguyên liệu vào)
- ESBMC = Thanh tra (kiểm tra quy trình)

## 8. VÍ DỤ THỰC TẾ

### 8.1. Không có test harness

```bash
$ esbmc fuzzgoat.c
ERROR: No main function found
```

### 8.2. Có test harness đơn giản

```c
// test.c
int main() {
    json_value *v = NULL;
    json_value_free(v);  // Test với NULL
    return 0;
}
```

```bash
$ esbmc test.c fuzzgoat.c
VERIFICATION SUCCESSFUL  # Không có lỗi với NULL
```

### 8.3. Có test harness với symbolic values

```c
// test_symbolic.c
int main() {
    int type = nondet_int();  // ← Symbolic!
    __ESBMC_assume(type >= 0 && type <= 7);
    
    json_value *v = create_value(type);
    json_value_free(v);
    return 0;
}
```

```bash
$ esbmc test_symbolic.c fuzzgoat.c
VERIFICATION FAILED  # Phát hiện lỗi!
  file fuzzgoat.c line 258
  dereference failure: array bounds violated
```

**→ Đây là sự khác biệt!** Với symbolic values, ESBMC khám phá tất cả cases và phát hiện lỗi.

