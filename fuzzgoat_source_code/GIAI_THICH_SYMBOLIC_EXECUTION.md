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

### 4.1. Cấu trúc json_value là gì?

Trước khi hiểu test harness, cần biết cấu trúc `json_value`:

```c
typedef struct _json_value {
    struct _json_value *parent;
    json_type type;  // Loại: json_array, json_object, json_string, ...
    
    union {
        // Nếu type = json_array
        struct {
            unsigned int length;
            struct _json_value **values;  // Mảng các phần tử
        } array;
        
        // Nếu type = json_object
        struct {
            unsigned int length;
            json_object_entry *values;  // Mảng các cặp key-value
        } object;
        
        // Nếu type = json_string
        struct {
            unsigned int length;
            char *ptr;  // Con trỏ đến chuỗi
        } string;
        
        // ... các loại khác
    } u;
} json_value;
```

**Quan trọng**: Đây là một **union** - chỉ một trong các trường (array, object, string) được sử dụng tùy theo `type`.

### 4.2. Test Harness thực tế - Từng bước chi tiết

Đây là code thực tế trong `test_esbmc_focused.c`:

#### Bước 1: Cấp phát bộ nhớ cho json_value

```c
int main() {
    // Tạo settings cho memory allocation
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;  // Hàm cấp phát bộ nhớ
    settings.mem_free = wrapper_free;    // Hàm giải phóng bộ nhớ
    
    // Cấp phát bộ nhớ cho một json_value
    // Đây là cấp phát THỰC SỰ, không phải symbolic
    json_value *value = (json_value *)malloc(sizeof(json_value));
    if (!value) {
        return 1;  // Nếu không cấp phát được thì thoát
    }
```

**Giải thích**: 
- `malloc(sizeof(json_value))` → Cấp phát bộ nhớ thực sự cho một cấu trúc `json_value`
- Kích thước: khoảng vài chục bytes (tùy theo kiến trúc)
- Con trỏ `value` trỏ đến vùng nhớ này

#### Bước 2: Tạo type SYMBOLIC

```c
    // Tạo type SYMBOLIC - ESBMC sẽ khám phá TẤT CẢ các giá trị có thể
    int type_choice = nondet_int();  // ← ĐÂY! Tạo biến symbolic
    __ESBMC_assume(type_choice >= 0 && type_choice <= 7);  // Giới hạn: 0-7
    
    value->type = (json_type)type_choice;  // Gán type symbolic vào cấu trúc
    value->parent = NULL;  // Không có parent (là root)
```

**Giải thích**:
- `nondet_int()` → ESBMC tạo một biến **symbolic** (không phải giá trị cụ thể)
- `__ESBMC_assume(...)` → Giới hạn phạm vi: type chỉ có thể là 0, 1, 2, ..., 7
- `value->type = (json_type)type_choice` → Gán type symbolic vào cấu trúc

**ESBMC hiểu**: "type có thể là bất kỳ giá trị nào từ 0 đến 7, tôi sẽ khám phá tất cả!"

#### Bước 3: Khởi tạo cấu trúc theo từng type

```c
    // Dựa vào type, khởi tạo các trường tương ứng
    switch (value->type) {
        case json_array: {
            // Nếu type là array, khởi tạo trường array
            unsigned int arr_len = nondet_int();  // ← Length cũng là symbolic!
            __ESBMC_assume(arr_len <= 5);  // Giới hạn: 0-5
            
            value->u.array.length = arr_len;  // Gán length symbolic
            
            if (arr_len > 0) {
                // Nếu length > 0, cấp phát mảng các con trỏ
                value->u.array.values = (json_value **)malloc(
                    arr_len * sizeof(json_value *)
                );
                // Khởi tạo tất cả con trỏ = NULL
                for (int i = 0; i < arr_len; i++) {
                    value->u.array.values[i] = NULL;
                }
            } else {
                // Nếu length = 0, không cấp phát
                value->u.array.values = NULL;
            }
            break;
        }
        
        case json_object: {
            // Tương tự cho object
            unsigned int obj_len = nondet_int();
            __ESBMC_assume(obj_len <= 5);
            value->u.object.length = obj_len;
            // ... cấp phát và khởi tạo
            break;
        }
        
        case json_string: {
            // Tương tự cho string
            unsigned int str_len = nondet_int();
            __ESBMC_assume(str_len <= 5);
            value->u.string.length = str_len;
            // ... cấp phát và khởi tạo
            break;
        }
    }
```

**Giải thích chi tiết**:

1. **Switch case dựa trên type symbolic**:
   - ESBMC sẽ khám phá TẤT CẢ các nhánh: `json_array`, `json_object`, `json_string`, ...
   - Mỗi nhánh tạo một cấu trúc khác nhau

2. **Với mỗi type, tạo length symbolic**:
   - `arr_len = nondet_int()` → Length cũng là symbolic
   - `__ESBMC_assume(arr_len <= 5)` → Giới hạn để tránh vòng lặp quá dài

3. **Cấp phát bộ nhớ thực sự**:
   - `malloc(arr_len * sizeof(json_value *))` → Cấp phát mảng thực sự
   - Nhưng `arr_len` là symbolic → ESBMC sẽ khám phá các trường hợp: length=0, 1, 2, 3, 4, 5

4. **Khởi tạo dữ liệu**:
   - Với array: Khởi tạo tất cả con trỏ = NULL
   - Với string: Khởi tạo chuỗi với ký tự 'A'

#### Bước 4: Gọi hàm cần test

```c
    // Gọi hàm json_value_free_ex với cấu trúc đã tạo
    // ESBMC sẽ tự động kiểm tra TẤT CẢ các paths trong hàm này
    json_value_free_ex(&settings, value);
    
    return 0;
}
```

### 4.3. ESBMC làm gì với test harness này?

#### Bước 1: Phân tích symbolic values

ESBMC nhận diện:
- `type_choice` là symbolic, có thể là: 0, 1, 2, 3, 4, 5, 6, 7
- `arr_len` là symbolic, có thể là: 0, 1, 2, 3, 4, 5
- `obj_len` là symbolic, có thể là: 0, 1, 2, 3, 4, 5
- `str_len` là symbolic, có thể là: 0, 1, 2, 3, 4, 5

#### Bước 2: Khám phá tất cả paths

ESBMC tạo một cây quyết định:

```
type = 0 (json_none)
  → Không có gì đặc biệt
  → Test path 1

type = 1 (json_object)
  → obj_len = 0 → Test empty object
  → obj_len = 1 → Test object with 1 entry
  → obj_len = 2 → Test object with 2 entries
  → ...
  → obj_len = 4 → Test object with 4 entries → PHÁT HIỆN LỖI! (dòng 258)

type = 2 (json_array)
  → arr_len = 0 → Test empty array
  → arr_len = 1 → Test array with 1 element
  → ...

type = 5 (json_string)
  → str_len = 0 → Test empty string → PHÁT HIỆN LỖI! (dòng 279)
  → str_len = 1 → Test one-byte string → PHÁT HIỆN LỖI! (dòng 298)
  → str_len = 2 → Test two-byte string
  → ...
```

#### Bước 3: Với mỗi path, kiểm tra lỗi

Khi ESBMC khám phá path `type=1, obj_len=4`:
1. Tạo cấu trúc: `value->type = json_object`, `value->u.object.length = 4`
2. Cấp phát mảng: `value->u.object.values = malloc(4 * sizeof(...))`
3. Gọi `json_value_free_ex(&settings, value)`
4. Trong hàm `json_value_free_ex`, tại dòng 258:
   ```c
   value = value->u.object.values [value->u.object.length--].value;
   ```
5. ESBMC phát hiện: `length--` được thực hiện TRƯỚC khi truy cập array
6. Khi `length=4`, code cố truy cập `values[4]` nhưng mảng chỉ có 0-3
7. → **PHÁT HIỆN LỖI**: `dereference failure: array bounds violated`

### 4.4. Tại sao cần tạo cấu trúc như vậy?

**Câu hỏi**: "Tại sao không chỉ gọi `json_value_free_ex(NULL)`?"

**Trả lời**:
1. **Hàm cần cấu trúc hợp lệ**: `json_value_free_ex` cần một con trỏ đến cấu trúc `json_value` hợp lệ
2. **Cần khởi tạo đúng**: Mỗi type cần cấu trúc khác nhau (array cần mảng, string cần chuỗi)
3. **Cần cấp phát bộ nhớ**: Các con trỏ trong cấu trúc cần trỏ đến vùng nhớ hợp lệ
4. **ESBMC cần symbolic values**: Để khám phá tất cả các trường hợp có thể

**Ví dụ minh họa**:

```c
// ❌ SAI: Không có cấu trúc
json_value_free_ex(&settings, NULL);  // Sẽ crash hoặc không test được gì

// ✅ ĐÚNG: Có cấu trúc nhưng type và length là symbolic
json_value *value = malloc(sizeof(json_value));
value->type = nondet_int();  // Symbolic!
value->u.array.length = nondet_int();  // Symbolic!
json_value_free_ex(&settings, value);  // ESBMC khám phá tất cả!
```

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

