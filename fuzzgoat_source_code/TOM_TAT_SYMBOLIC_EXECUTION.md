# TÓM TẮT: TẠI SAO CẦN TEST HARNESS VỚI SYMBOLIC EXECUTION?

## 🎯 CÂU TRẢ LỜI NGẮN GỌN

**ESBMC KHÔNG tự động biết đâu là input!** 

Bạn phải **chỉ định rõ ràng** trong test harness:
- ✅ Biến nào là **symbolic** (input từ bên ngoài)
- ✅ Hàm nào cần được **gọi**
- ✅ Cấu trúc dữ liệu nên được **tạo như thế nào**

## 📊 MINH HỌA BẰNG VÍ DỤ

### ❌ Test 1: Không có test harness

```bash
$ esbmc demo_symbolic.c
ERROR: main symbol `main' not found
```

**→ ESBMC không biết bắt đầu từ đâu!**

### ⚠️ Test 2: Test harness với concrete values

```c
int main() {
    int result = divide(10, 2);  // Chỉ test với giá trị cụ thể
}
```

```bash
$ esbmc demo_test_concrete.c demo_symbolic.c
VERIFICATION SUCCESSFUL
```

**→ Không phát hiện lỗi!** Vì chỉ test với `b=2` (không phải `b=0`)

### ✅ Test 3: Test harness với symbolic values

```c
int main() {
    int a = nondet_int();  // ← CHỈ ĐỊNH: a là symbolic
    int b = nondet_int();   // ← CHỈ ĐỊNH: b là symbolic
    int result = divide(a, b);
}
```

```bash
$ esbmc demo_test_symbolic.c demo_symbolic.c
VERIFICATION FAILED
  division by zero
  b != 0
```

**→ PHÁT HIỆN LỖI!** Vì ESBMC khám phá TẤT CẢ giá trị có thể, bao gồm `b=0`

## 🔑 ĐIỂM QUAN TRỌNG

### 1. Symbolic Execution ≠ Tự động hoàn toàn

- ✅ **Tự động**: Khám phá tất cả paths từ symbolic inputs
- ❌ **KHÔNG tự động**: Nhận diện đâu là input
- ❌ **KHÔNG tự động**: Biết cách gọi hàm

### 2. Test Harness = "Hướng dẫn" cho ESBMC

Test harness cho ESBMC biết:
1. **Entry point**: Hàm `main()` - nơi bắt đầu
2. **Symbolic inputs**: `nondet_int()`, `nondet_char()` - biến nào là symbolic
3. **Test scenario**: Gọi hàm nào, với tham số gì

### 3. Với fuzzgoat.c

```c
// fuzzgoat.c chỉ có các hàm:
json_value * json_parse(...) { ... }
void json_value_free_ex(...) { ... }
```

**ESBMC không biết:**
- ❌ Gọi hàm nào? (`json_parse` hay `json_value_free_ex`?)
- ❌ Với tham số gì?
- ❌ Cấu trúc `json_value` nên tạo như thế nào?

**→ CẦN test harness để chỉ định!**

```c
// test_esbmc_focused.c
int main() {
    // Chỉ định: type là symbolic
    int type = nondet_int();
    __ESBMC_assume(type >= 0 && type <= 7);
    
    // Tạo json_value với type symbolic
    json_value *v = create_value(type);
    
    // Gọi hàm cần test
    json_value_free_ex(&settings, v);
}
```

## 💡 KẾT LUẬN

**Test harness KHÔNG phải là "test case cụ thể"!**

Test harness là:
- ✅ **Định nghĩa** đâu là input (symbolic)
- ✅ **Setup** môi trường (cấu trúc dữ liệu)
- ✅ **Hướng dẫn** ESBMC cách test

**ESBMC vẫn tự động:**
- ✅ Khám phá TẤT CẢ giá trị có thể của symbolic inputs
- ✅ Tìm TẤT CẢ paths thực thi
- ✅ Phát hiện lỗi mà không cần biết trước lỗi là gì

**Ví dụ:**
- Test harness: "type có thể là 0, 1, 2, ..., 7" (symbolic)
- ESBMC: Tự động khám phá tất cả 8 cases
- ESBMC: Phát hiện lỗi khi `type=1, length=4` → **Không cần biết trước lỗi này!**

## 📚 TƯƠNG TỰ VỚI CÁC TOOL KHÁC

- **AFL**: Cần chỉ định input directory (`-i input_dir`)
- **Valgrind**: Cần chạy program (`valgrind ./program`)
- **ESBMC**: Cần test harness (định nghĩa symbolic inputs)

**Tất cả đều cần "hướng dẫn" từ developer!**

