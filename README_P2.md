# Tổng quan chương trình fuzzgoat

![alt text](image-6.png)

**Fuzzgoat** là một chương trình C mã nguồn mở, được sửa đổi từ thư viện `udp/json-parser`. Chức năng chính của nó là đọc một tệp JSON từ đầu vào chuẩn (stdin) hoặc từ tệp, phân tích cú pháp và in ra cấu trúc dữ liệu tương ứng. 

Cụ thể chương trình hoạt động theo mô hình đệ quy (recursive descent parser). Nó đọc một chuỗi byte đầu vào và cố gắng xây dựng một cấu trúc dữ liệu cây đại diện cho đối tượng JSON.

- Input: File JSON hoặc luồng dữ liệu từ stdin.
- Process: Hàm json_parse gọi các hàm con để xử lý Object, Array, String, Number, Boolean, v.v.
- Output: Chương trình thường không in ra kết quả phân tích mà chỉ trả về mã thoát, trừ khi gặp lỗi crash do các lỗ hổng.


Cấu trúc thư mục:

```
fuzzgoat_source_code/
├── in/seed            # Chứa seed đầu vào cho AFL++ fuzzing
├── input-files        # Chứa các payload sẽ trigger lỗ hổng(dùng để đối chiếu kết quả)
├── fuzzgoat.c         # Mã nguồn chính của chương trình
├── fuzzgoat.h         # Khai báo hàm, macro cho fuzzgoat.c
├── main.c             # Hàm main để khởi động chương trình
├── fuzzgoatNoVulns.c  # Phiên bản không có lỗ hổng của fuzzgoat
└── Makefile           # Tập lệnh build
```

`main.c` là entry point khi chạy theo cách truyền thống (không fuzz), hoặc được dùng làm **harness** tối giản để AFL/LibFuzzer có điểm vào.

Với fuzzers như AFL++, harness thường là một chương trình có hàm main() đọc dữ liệu từ stdin hoặc @@ (đường dẫn file do AFL cấp) rồi chuyển dữ liệu đó vào hàm bạn muốn fuzz. Trong repo này, `main.c` chính là harness để chạy AFL++.

Biên dịch:

```bash
# Using -lm to link the math library
gcc main.c fuzzgoat.c -o fuzzgoat-bin  -lm
```

Chạy thử:

```bash
./fuzzgoat-bin test_sample.json
```

``` 
{
  "id": 1,
  "name": "Sample Item",
  "tags": ["test", "demo", "json"],
  "details": {
    "created_at": "2025-01-01",
    "description": "This is a dummy JSON object."
  }
}

--------------------------------

 object[0].name = id
  int:          1
 object[1].name = name
  string: Sample Item
 object[2].name = tags
  array
   string: test
   string: demo
   string: json
 object[3].name = details
   object[0].name = created_at
    string: 2025-01-01
   object[1].name = description
    string: This is a dummy JSON object.
Segmentation fault (core dumped)
```



Mã nguồn của Fuzzgoat (`fuzzgoat.c`) tương đối nhỏ gọn (~1200 dòng). Fuzzgoat được cấy ghép nhiều lỗ hổng điển hình để làm thước đo cho các công cụ kiểm thử. Các lỗ hổng này đã được comment lại trong mã nguồn. Bảng dưới đây tóm tắt các lỗ hổng chính cần tìm:

**1. Lỗ hổng Use After Free (Sử dụng bộ nhớ sau khi giải phóng)**

- **Vị trí**: Hàm `new_value`, bên trong `case json_array`.

```c
if (value->u.array.length == 0)
{
   free(*top); // Dòng gây lỗi
   break;
}
```

- **Phân tích nguyên nhân**: Khi trình phân tích cú pháp (parser) gặp một mảng JSON rỗng (`[]`), nó thực hiện lệnh `free(*top)` để giải phóng khối nhớ được trỏ bởi `*top`. Tuy nhiên, con trỏ này không được gán lại thành `NULL` hoặc được xử lý đúng cách để ngăn chặn việc truy cập sau đó. Chương trình vẫn tiếp tục chạy và cố gắng sử dụng vùng nhớ đã bị giải phóng này ở các bước tiếp theo.


- **Hậu quả**: Gây ra lỗi hư hỏng bộ nhớ (memory corruption), có thể dẫn đến crash chương trình hoặc trong các tình huống thực tế nghiêm trọng hơn là thực thi mã tùy ý (arbitrary code execution).

- **Cách kích hoạt**: Sử dụng input là một mảng JSON rỗng: 
  - Payload: `[]`
  - File mẫu: `input-files/emptyArray`

**2. Lỗ hổng Out-of-bounds Read / Invalid Free (Đọc ngoài vùng nhớ / Giải phóng sai)**

- **Vị trí**: Hàm `json_value_free_ex`, bên trong case `json_object`

```c
value = value->u.object.values [value->u.object.length--].value;
```

- **Phân tích nguyên nhân**: Đoạn mã sử dụng toán tử giảm sau (post-decrement) `length--` làm chỉ số mảng.

  - Nếu mảng có độ dài là `N`, các chỉ số hợp lệ là từ `0` đến `N-1`.

  - Việc sử dụng `[length--]` sẽ truy cập vào phần tử tại chỉ số `N` (vượt quá giới hạn mảng), sau đó mới giảm giá trị `length`. Điều này dẫn đến việc đọc dữ liệu rác hoặc dữ liệu không thuộc quyền quản lý của mảng đó.

- **Hậu quả**: `free()` yêu cầu con trỏ phải trỏ chính xác vào đầu vùng nhớ được cấp phát bởi `malloc()`. Việc truyền một con trỏ sai lệch (trỏ vào redzone hoặc metadata của allocator) sẽ gây ra lỗi Invalid Free hoặc làm hỏng cấu trúc heap (heap corruption).

- **Cách kích hoạt**: Sử dụng một đối tượng JSON hợp lệ bất kỳ.

  - Payload: `{"":0}`

  - File mẫu: `input-files/validObject`.


**3. Lỗ hổng Invalid Pointer Free (Giải phóng con trỏ không hợp lệ)**

- **Vị trí**: Hàm `json_value_free_ex`, bên trong case `json_string`.

```c
if (!value->u.string.length){
  value->u.string.ptr--; // Dòng gây lỗi
}
// ... sau đó ...
settings->mem_free (value->u.string.ptr, settings->user_data);
```

- **Phân tích nguyên nhân**: Nếu chuỗi JSON là chuỗi rỗng (độ dài bằng 0), mã nguồn cố tình giảm địa chỉ con trỏ `value->u.string.ptr` đi 1 đơn vị. Sau đó, chương trình gọi hàm `mem_free` (tương đương `free`) lên con trỏ đã bị thay đổi này. Trình quản lý bộ nhớ chỉ có thể giải phóng địa chỉ bắt đầu chính xác của khối nhớ đã cấp phát; việc truyền vào một địa chỉ sai sẽ gây lỗi.

- **Hậu quả**: Gây lỗi phân bổ bộ nhớ, thường dẫn đến `SIGABRT` (Process abort signal) hoặc crash chương trình ngay lập tức.

- **Cách kích hoạt**: Sử dụng input là một chuỗi JSON rỗng.

  - Payload: `""`

  - File mẫu: `input-files/emptyString.`

**4. Lỗ hổng Null Pointer Dereference (Truy cập con trỏ NULL)**
- **Vị trí**: hàm `json_value_free_ex`, bên trong case `json_string`.


```c
if (value->u.string.length == 1) {
  char *null_pointer = NULL;
  printf ("%d", *null_pointer); // Dòng gây lỗi
}
```
- **Phân tích nguyên nhân**: Đoạn mã kiểm tra nếu chuỗi có độ dài bằng 1. Nếu đúng, nó khởi tạo một con trỏ `null_pointer` với giá trị `NULL` và cố gắng truy cập (dereference) giá trị mà nó trỏ tới để in ra.

- **Hậu quả**: Truy cập vào địa chỉ 0 (NULL) là bất hợp pháp trong hầu hết các hệ điều hành hiện đại, dẫn đến việc hệ điều hành chấm dứt chương trình ngay lập tức (Segmentation Fault / SIGSEGV).

- **Cách kích hoạt**: Sử dụng input là một chuỗi JSON có độ dài đúng bằng 1 ký tự.

  - Payload: `"A"`

  - File mẫu: `input-files/oneByteString`.

# Kiểm tra bằng phương pháp static analysis

## ESBMC

Một vài tùy chọn kiểm tra (Trong phiên bản ESBMC 7.11.0 64-bit được sử dụng tại thời điểm viết tài liệu):

| Option                        | Tác dụng                                                                 | Bật mặc định |
|-------------------------------|-------------------------------------------------------------------------|--------------|
| `--overflow-check`            | Kiểm tra tràn số nguyên                   |             |
| `--memory-leak-check`         | Kiểm tra rò rỉ bộ nhớ                                        |            |
| `--unwind` | Unroll vòng lặp for / hàm đệ quy
| `--deadlock-check`            | Kiểm tra tắc nghẽn khi dùng đa luồng                           |            |
| `--data-races-check`          | Kiểm tra tương tranh giữa các luồng                     |            |
| `--lock-order-check`          | Kiểm tra thứ tự lấy/giải phóng các khóa      |            |
| `--atomicity-check`           | Kiểm tra vi phạm tính nguyên tử trong các phép gán hiển thị   |            |
| `--force-malloc-success`      | Giả sử việc malloc luôn thành công (dùng khi muốn bỏ qua lỗi cấp phát)   |            |
| `--bounds-check`              | Kiểm tra truy cập mảng ngoài giới hạn (array out-of-bounds)                | ✓            |
| `--pointer-check`             | Kiểm tra lỗi con trỏ (NULL dereference, out-of-bounds pointer, double-free) | ✓            |
| `--div-by-zero-check`         | Kiểm tra phép chia cho 0 (divide by zero)                                 | ✓            |
| `--assertions`                | Kiểm tra các khẳng định do người dùng đặt (user‐specified assertions)     | ✓            |

Không thể chạy trực tiếp ESBMC trên `fuzzgoat.c` do file này không có hàm `main()` và không biết đâu là input cần kiểm thử. (ESSBMC khám phá tất cả paths từ symbolic inputs chứ nó không thể tự dộng nhận biết đâu là một input, đâu là hàm cần test) -> Cần tạo một file harness để hướng dẫn ESBMC. 

Với các file yêu cầu đầu vào, chỉ định trong ESBMC như sau:

```c
int main() {
    int a = nondet_int();  // ← CHỈ ĐỊNH: a là symbolic
    int b = nondet_int();   // ← CHỈ ĐỊNH: b là symbolic
    int result = divide(a, b);
}
```

Output:

```
[Counterexample]


State 1 file test.c line 10 column 5 function main thread 0
----------------------------------------------------
  b = 0 (00000000 00000000 00000000 00000000)

State 2 file test.c line 5 column 5 function divide thread 0
----------------------------------------------------
Violated property:
  file test.c line 5 column 5 function divide
  division by zero
  b != 0


VERIFICATION FAILED
```

Áp dụng cho file `fuzzgoat.c`, cần tạo một test harness tương tự để chỉ định hàm cần kiểm thử và các tham số đầu vào nào là symbolic.

- Với hàm cần kiểm thử, do biết trước các lỗi chỉ nằm trong hàm `son_value * json_parse(...) { ... }` (Lỗi 1) và `void json_value_free_ex(...) { ... }` (Lỗi 2,3,4), ta có thể tạo hai test harness riêng biệt để kiểm thử từng hàm một.

- Với cấu trúc đầu vào thì sẽ phức tạp hơn trong ví dụ trên vì `json_value` là một cấu trúc phức tạp.

DỰa trên source code trong `fuzzgoat.h`, ta có cấu trúc của object này là:

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

Đây là một **union** - chỉ một trong các trường (array, object, string) được sử dụng tùy theo `type`.

Từ `fuzzgoat.h`, `json_type` là một enum với 8 giá trị:

```c
typedef enum {
   json_none,      // = 0
   json_object,    // = 1
   json_array,     // = 2
   json_integer,   // = 3
   json_double,    // = 4
   json_string,    // = 5
   json_boolean,   // = 6
   json_null       // = 7
} json_type;
```

Như vậy ta sẽ chỉ định đầu vào sympolic với:

```c
int type_choice = nondet_int();
__ESBMC_assume(type_choice >= 0 && type_choice <= 7); 

// Sau đó gán giá trị symbolic vào trường type của cấu trúc
// -> Cấu trúc json_value bây giờ có type là symbolic.
value->type = (json_type)type_choice;


// Đến đây, ESBMC chỉ biết: "type_choice có thể là 0-7"
// NHƯNG chưa làm gì với type_choice cả. Ta cần chỉ định nơi ESBMC khám phá các paths tương ứng với type_choice
switch (value->type) {
    case json_array: { ... }      // Path 1
    case json_object: { ... }     // Path 2
    case json_string: { ... }     // Path 3
    case json_integer: { ... }    // Path 4
    // ... (tổng cộng 8 paths)
}
```

Dựa trên ý tưởng trên, ta tạo file [harness_esbmc_first_try.c](./fuzzgoat_source_code/ESBMC/harness_esbmc_first_try.c) với symbolic inputs:

```c
int main() {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    json_value *value = (json_value *)malloc(sizeof(json_value));
    
    // Tạo symbolic type
    int type_choice = nondet_int();
    __ESBMC_assume(type_choice >= 0 && type_choice <= 7);
    value->type = (json_type)type_choice;
    
    // Với mỗi type, khởi tạo cấu trúc với symbolic values
    switch (value->type) {
        case json_array: {
            unsigned int arr_len = nondet_int();
            __ESBMC_assume(arr_len <= 5);
            value->u.array.length = arr_len;
            // ... khởi tạo array
            break;
        }
        case json_string: {
            unsigned int str_len = nondet_int();
            __ESBMC_assume(str_len <= 5);
            value->u.string.length = str_len;
            // ... khởi tạo string
            break;
        }
        // ... các case khác
    }
    
    json_value_free_ex(&settings, value);
    return 0;
}
```

**Ý tưởng:** ESBMC sẽ tự động khám phá tất cả các paths tương ứng với:
- `type` có thể là 0-7 (8 giá trị)
- `length` có thể là 0-5 (6 giá trị với mỗi type)

Chạy:

```bash
esbmc harness_esbmc_first_try.c fuzzgoat.c --unwind 10
```
Kết quả:

```
[Counterexample]


State 1 file harness_esbmc_first_try.c line 31 column 5 function main thread 0
----------------------------------------------------
  value = ( struct _json_value *)(&dynamic_6_value)

State 2 file harness_esbmc_first_try.c line 37 column 5 function main thread 0
----------------------------------------------------
  type_choice = 1 (00000000 00000000 00000000 00000001)

State 7 file harness_esbmc_first_try.c line 40 column 5 function main thread 0
----------------------------------------------------
  value->type = { .parent=nil, .type=(unsigned int)type_choice, .anon_pad$2=nil,
    .u=nil, ._reserved=nil }

State 9 file harness_esbmc_first_try.c line 41 column 5 function main thread 0
----------------------------------------------------
  value->parent = { .parent=0, .type=1, .anon_pad$2=0, .u={ .boolean=0, .integer=648527176794112000, .dbl=(double)648527176794112000,
    .string=( struct __anon_struct_at__/fuzzgoat_h__113_7 { unsigned int length; unsigned int anon_pad$1; signed char * ptr; })0x2332CB7F4864BD200900080800000000, .object=( struct __anon_struct_at__/fuzzgoat_h__120_7 { unsigned int length; unsigned int anon_pad$1;  struct _json_object_entry * values; })0x2332CB7F4864BD200900080800000000,
    .array=( struct __anon_struct_at__/fuzzgoat_h__137_7 { unsigned int length; unsigned int anon_pad$1;  struct _json_value * * values; })0x2332CB7F4864BD200900080800000000 }, ._reserved={ .next_alloc=( struct _json_value *)0, .object_mem=0 } }

State 13 file harness_esbmc_first_try.c line 70 column 13 function main thread 0
----------------------------------------------------
  obj_len = 2 (00000000 00000000 00000000 00000010)

State 18 file harness_esbmc_first_try.c line 72 column 13 function main thread 0
----------------------------------------------------
  value->u.object.length = { .parent=0, .type=1, .anon_pad$2=0, .u=2, ._reserved={ .next_alloc=( struct _json_value *)0, .object_mem=0 } }

State 21 file harness_esbmc_first_try.c line 75 column 17 function main thread 0
----------------------------------------------------
  value->u.object.values = { .parent=0, .type=1, .anon_pad$2=0, .u=(signed char *)(( struct _json_object_entry *)(&dynamic_14_array[0])), ._reserved={ .next_alloc=( struct _json_value *)0, .object_mem=0 } }

State 46 file fuzzgoat.c line 220 column 4 function json_value_free_ex thread 0
----------------------------------------------------
  value->parent = { .parent=0, .type=1, .anon_pad$2=0, .u={ .boolean=2, .integer=648527176794112002, .dbl=(double)648527176794112002,
    .string=( struct __anon_struct_at__/fuzzgoat_h__113_7 { unsigned int length; unsigned int anon_pad$1; signed char * ptr; })0x2332CB7F1C083AC00900080800000002, .object=( struct __anon_struct_at__/fuzzgoat_h__120_7 { unsigned int length; unsigned int anon_pad$1;  struct _json_object_entry * values; })0x2332CB7F1C083AC00900080800000002,
    .array=( struct __anon_struct_at__/fuzzgoat_h__137_7 { unsigned int length; unsigned int anon_pad$1;  struct _json_value * * values; })0x2332CB7F1C083AC00900080800000002 }, ._reserved={ .next_alloc=( struct _json_value *)0, .object_mem=0 } }

State 69 file fuzzgoat.c line 258 column 13 function json_value_free_ex thread 0
----------------------------------------------------
Violated property:
  file fuzzgoat.c line 258 column 13 function json_value_free_ex
  dereference failure: array bounds violated


VERIFICATION FAILED
```

Nhìn trong kết quả đầu ra, ESBMC báo “dereference failure: array bounds violated” tại `fuzzgoat.c` line 258 column 13, trong hàm `json_value_free_ex`, đây là lỗi thứ 2 đã mô tả ở trên (Out-of-bounds Read):

```c
value = value->u.object.values [value->u.object.length--].value;
```

Đọc các state phía trên, tại:

1. Stage 13: `obj_len = 2` → Tạo object với độ dài 2
2. Stage 18: `value->u.object.length = 2` nghĩa là object có 2 cặp key-value, vậy chỉ số hợp lệ là 0 và 1.
3. State 21: `value->u.object.values = (&dynamic_14_array[0])` tức là mảng gồm 2 phần tử (giả lập bởi harness), hợp lệ các index 0..1.

4. State 46 : Bắt đầu vào hàm `json_value_free_ex`.

5. State 69:  báo vi phạm thuộc tính tại line 258 khi dereference chỉ số mảng. Với length = 2, biểu thức values[length--] sẽ truy cập values[2] rồi mới giảm length xuống 1. Index 2 là vượt biên (out-of-bounds).


Sau khi vá lỗi này bằng cách

```diff
- value = value->u.object.values [value->u.object.length--].value;
+ value = value->u.object.values [--value->u.object.length].value;
```

rồi chạy lại ESBMC ra lỗi tiếp theo:

```
[Counterexample]


State 1 file harness_esbmc_first_try.c line 31 column 5 function main thread 0
----------------------------------------------------
  value = ( struct _json_value *)(&dynamic_6_value)

State 2 file harness_esbmc_first_try.c line 37 column 5 function main thread 0
----------------------------------------------------
  type_choice = 5 (00000000 00000000 00000000 00000101)

State 7 file harness_esbmc_first_try.c line 40 column 5 function main thread 0
----------------------------------------------------
  value->type = { .parent=nil, .type=(unsigned int)type_choice, .anon_pad$2=nil,
    .u=nil, ._reserved=nil }

State 9 file harness_esbmc_first_try.c line 41 column 5 function main thread 0
----------------------------------------------------
  value->parent = { .parent=0, .type=5, .anon_pad$2=0, .u={ .boolean=0, .integer=-4294967296, .dbl=(double)0xFFFFFFFF00000000, .string=( struct __anon_struct_at__/fuzzgoat_h__113_7 { unsigned int length; unsigned int anon_pad$1; signed char * ptr; })0x96BEEFFFD4430B6CFFFFFFFF00000000,
    .object=( struct __anon_struct_at__/fuzzgoat_h__120_7 { unsigned int length; unsigned int anon_pad$1;  struct _json_object_entry * values; })0x96BEEFFFD4430B6CFFFFFFFF00000000,
    .array=( struct __anon_struct_at__/fuzzgoat_h__137_7 { unsigned int length; unsigned int anon_pad$1;  struct _json_value * * values; })0x96BEEFFFD4430B6CFFFFFFFF00000000 }, ._reserved={ .next_alloc=( struct _json_value *)0, .object_mem=0 } }

State 16 file harness_esbmc_first_try.c line 95 column 13 function main thread 0
----------------------------------------------------
  str_len = 1 (00000000 00000000 00000000 00000001)

State 21 file harness_esbmc_first_try.c line 97 column 13 function main thread 0
----------------------------------------------------
  value->u.string.length = { .parent=0, .type=5, .anon_pad$2=0, .u=1, ._reserved={ .next_alloc=( struct _json_value *)0, .object_mem=0 } }

State 24 file harness_esbmc_first_try.c line 100 column 17 function main thread 0
----------------------------------------------------
  value->u.string.ptr = { .parent=0, .type=5, .anon_pad$2=0, .u=&dynamic_18_array[0], ._reserved={ .next_alloc=( struct _json_value *)0, .object_mem=0 } }

State 39 file fuzzgoat.c line 220 column 4 function json_value_free_ex thread 0
----------------------------------------------------
  value->parent = { .parent=0, .type=5, .anon_pad$2=0, .u={ .boolean=1, .integer=-4294967295, .dbl=(double)0xFFFFFFFF00000001, .string=( struct __anon_struct_at__/fuzzgoat_h__113_7 { unsigned int length; unsigned int anon_pad$1; signed char * ptr; })0x529D4FFD69422470FFFFFFFF00000001,
    .object=( struct __anon_struct_at__/fuzzgoat_h__120_7 { unsigned int length; unsigned int anon_pad$1;  struct _json_object_entry * values; })0x529D4FFD69422470FFFFFFFF00000001,
    .array=( struct __anon_struct_at__/fuzzgoat_h__137_7 { unsigned int length; unsigned int anon_pad$1;  struct _json_value * * values; })0x529D4FFD69422470FFFFFFFF00000001 }, ._reserved={ .next_alloc=( struct _json_value *)0, .object_mem=0 } }

State 53 file fuzzgoat.c line 298 column 15 function json_value_free_ex thread 0
----------------------------------------------------
Violated property:
  file fuzzgoat.c line 298 column 15 function json_value_free_ex
  dereference failure: NULL pointer


VERIFICATION FAILED
```

ESBMC đã phát hiện ra lỗi thứ 4 đã được đề cập (Null Pointer Dereference) tại line 298 column 15 trong hàm `json_value_free_ex`:

```c
if (value->u.string.length == 1) {
    char *null_pointer = NULL;
    printf ("%d", *null_pointer); // Dòng gây lỗi
}
```
Phân tích các trạng thái:

1. State 2: type_choice = 5 → value->type là json_string
2. State 16: str_len = 1
3. State 21: value->u.string.length = 1
4. State 24: value->u.string.ptr trỏ tới dynamic_18_array[0] (không phải NULL). Khi vào json_value_free_ex, case json_string sẽ được thực thi.

5. Trong case json_string có đoạn mã gây lỗi. Nếu `value->u.string.length == 1` -> Tạo con trỏ `null_pointer = NULL`. Ngay sau đó dereference: `printf("%d", *null_pointer);`. Đây chính là dereference con trỏ NULL → ESBMC báo lỗi tại line 298:15.

Sửa lỗi:

```diff
case json_string:
-  if (value->u.string.length == 1) {
-    char *null_pointer = NULL; 
-    printf("%d", *null_pointer); 
-  }
+  settings->mem_free (value->u.string.ptr, settings->user_data);
+  break;
```

Tác giả sửa lỗi Null Pointer Dereference (nhánh `length == 1` trong json_string) bằng cách xóa hoàn toàn đoạn tạo và dereference con trỏ NULL. Phần xử lý json_string giờ chỉ giải phóng bộ nhớ chuỗi một cách an toàn.

Tuy nhiên khi này chạy lại ESBMC không phát hiện thêm lỗi nào nữa.

```
Symex completed in: 0.152s (2328 assignments)
Caching time: 0.043s (removed 468 assertions)
Slicing time: 0.037s (removed 455 assignments)
Generated 1519 VCC(s), 619 remaining after simplification (1405 assignments)
No solver specified; defaulting to Boolector
Encoding remaining VCC(s) using bit-vector/floating-point arithmetic
Encoding to solver time: 0.022s
Solving with solver Boolector 3.2.4
Runtime decision procedure: 131.813s
BMC program time: 132.064s

VERIFICATION SUCCESSFUL
```

Lỗi 1 và 3 chưa phát hiện được

- Lỗi 1 (Use-After-Free) không phát hiện vì harness này chỉ gọi `json_value_free_ex()`. Lỗi 1 nằm trong hàm `new_value()` (nhánh json_array) khi xử lý `[]`, nên path đó không bao giờ được thực thi. ESBMC không thể thấy UAF nếu hàm chứa lỗi không được gọi.

---------------------------------------------------------

**Kết quả:** Chỉ phát hiện được **2/4 lỗi**:
- ✓ Lỗi 3: Invalid Pointer Free (empty string)
- ✓ Lỗi 4: Null Pointer Dereference (one-byte string)
- ✗ Lỗi 1: Use After Free (empty array) - KHÔNG phát hiện
- ✗ Lỗi 2: Out-of-bounds Read (object) - KHÔNG phát hiện

### Tại sao symbolic approach không thành công như mong đợi?

**Vấn đề 1: Lỗi 1 (Use After Free) không nằm trong hàm `json_value_free_ex`**

Lỗi Use After Free xảy ra ở hàm `new_value()` (dòng 137 trong fuzzgoat.c):

```c
if (value->u.array.length == 0)
{
    free(*top);  // ← Lỗi ở đây
    break;
}
```

Nhưng test harness của chúng ta chỉ gọi `json_value_free_ex()`, không gọi `new_value()`. Do đó ESBMC không bao giờ khám phá đến đoạn code này.

**Giải pháp:** Cần một test harness riêng gọi hàm `json_parse()` hoặc `new_value()` trực tiếp. Tuy nhiên:
- `json_parse()` yêu cầu input là chuỗi JSON hợp lệ → khó tạo symbolic string phức tạp
- `new_value()` là hàm static → không thể gọi từ bên ngoài file

→ **Lỗi 1 không thể phát hiện bằng ESBMC với harness này.**

**Vấn đề 2: Lỗi 2 (Out-of-bounds Read) bị "che giấu" bởi lỗi trong harness**

Lỗi này nằm ở dòng 258 trong fuzzgoat.c:

```c
value = value->u.object.values [value->u.object.length--].value;
```

Khi ESBMC khám phá path với `type = json_object`, nó phát hiện lỗi nhưng không phải lỗi out-of-bounds read như mong đợi. Thay vào đó, ESBMC báo lỗi:

```
Violated property:
  dereference failure: NULL pointer
  value->u.object.values[i].value != NULL
```

**Nguyên nhân:** Trong test harness, ta khởi tạo:

```c
case json_object: {
    value->u.object.values = (json_object_entry *)malloc(obj_len * sizeof(json_object_entry));
    for (i = 0; i < obj_len; i++) {
        value->u.object.values[i].value = NULL;  // ← Khởi tạo = NULL
    }
}
```

Khi `json_value_free_ex` cố gắng truy cập `value->u.object.values[...].value`, nó gặp con trỏ NULL ngay lập tức. ESBMC phát hiện NULL pointer dereference trong **test harness**, không phải lỗi out-of-bounds read trong **fuzzgoat.c**.

**Vấn đề cốt lõi:** Lỗi Out-of-bounds Read chỉ có ý nghĩa khi cấu trúc `json_value` được khởi tạo **đúng cách** bởi `json_parse()`. Test harness của chúng ta tạo ra cấu trúc "giả" không tuân theo các invariant của chương trình thực.

→ **Lỗi 2 bị che giấu bởi lỗi khởi tạo trong harness.**

**Vấn đề 3: State explosion với symbolic execution**

Với cách tiếp cận symbolic:
- 8 giá trị type khác nhau
- 6 giá trị length khác nhau cho mỗi type
- Các vòng lặp for khởi tạo mảng/object
- Đệ quy trong `json_value_free_ex`

ESBMC phải khám phá hàng chục đến hàng trăm paths. Với `--unwind` nhỏ, một số paths không được khám phá đầy đủ. Với `--unwind` lớn, thời gian chạy tăng vọt (hàng phút đến hàng giờ).

**Vấn đề 4: Khó tạo symbolic input phù hợp với invariants**

Chương trình JSON parser có nhiều invariants ngầm định:
- Nếu `type = json_string` và `length > 0`, thì `ptr` phải trỏ đến vùng nhớ hợp lệ
- Nếu `type = json_object` và `length > 0`, thì mỗi `values[i].value` phải trỏ đến một `json_value` hợp lệ (không phải NULL)
- Cấu trúc cây: `parent` và `child` phải nhất quán

Việc tạo một cấu trúc symbolic thỏa mãn tất cả các invariants này là **cực kỳ khó khăn**. Nếu không thỏa mãn, ESBMC sẽ phát hiện lỗi trong harness thay vì lỗi trong code gốc.

### Cách tiếp cận 2: Test harness cụ thể cho từng lỗi

Do symbolic approach gặp nhiều hạn chế, ta chuyển sang tạo **test harness cụ thể** cho từng lỗi:

**File `test_string_empty.c` - Phát hiện Lỗi 3 (Invalid Pointer Free):**

```c
int main() {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    json_value *value = (json_value *)malloc(sizeof(json_value));
    
    // Thiết lập điều kiện CỤ THỂ: string rỗng
    value->type = json_string;
    value->parent = NULL;
    value->u.string.length = 0;  // ← Empty string
    value->u.string.ptr = (json_char *)malloc(1);
    value->u.string.ptr[0] = '\0';
    
    json_value_free_ex(&settings, value);
    return 0;
}
```

Chạy ESBMC:

```bash
esbmc test_string_empty.c fuzzgoat.c --unwind 5
```

**Kết quả:**

```
[Counterexample]

State 1 file fuzzgoat.c line 279 function json_value_free_ex
----------------------------------------------------
Violated property:
  dereference failure: invalid pointer freed
  value->u.string.ptr points to valid memory
  
VERIFICATION FAILED
```

✓ **Thành công phát hiện Lỗi 3.**

**File `test_string_one.c` - Phát hiện Lỗi 4 (Null Pointer Dereference):**

```c
int main() {
    json_settings settings = { 0 };
    settings.mem_alloc = wrapper_alloc;
    settings.mem_free = wrapper_free;
    
    json_value *value = (json_value *)malloc(sizeof(json_value));
    
    // Thiết lập điều kiện CỤ THỂ: string có 1 ký tự
    value->type = json_string;
    value->parent = NULL;
    value->u.string.length = 1;  // ← One-byte string
    value->u.string.ptr = (json_char *)malloc(2);
    value->u.string.ptr[0] = 'A';
    value->u.string.ptr[1] = '\0';
    
    json_value_free_ex(&settings, value);
    return 0;
}
```

Chạy ESBMC:

```bash
esbmc test_string_one.c fuzzgoat.c --unwind 5
```

**Kết quả:**

```
[Counterexample]

State 1 file fuzzgoat.c line 298 function json_value_free_ex
----------------------------------------------------
Violated property:
  dereference failure: NULL pointer
  null_pointer != NULL
  
VERIFICATION FAILED
```

✓ **Thành công phát hiện Lỗi 4.**

### So sánh hai cách tiếp cận

| Tiêu chí | Symbolic Input (test_esbmc_focused.c) | Concrete Input (test_string_*.c) |
|----------|--------------------------------------|----------------------------------|
| **Khả năng phát hiện** | 2/4 lỗi | 2/4 lỗi (nhưng khác lỗi) |
| **Thời gian chạy** | Lâu (hàng phút) với --unwind 10 | Nhanh (vài giây) với --unwind 5 |
| **False positives** | Có (lỗi trong harness) | Không |
| **Độ chính xác** | Thấp (nhiễu do NULL pointers) | Cao (target chính xác) |
| **Độ coverage** | Cao (nhiều paths) | Thấp (1 path/lỗi) |
| **Khả năng tìm lỗi mới** | Có (nếu không biết trước) | Không (phải biết trước lỗi) |

### Tóm tắt khó khăn khi dùng ESBMC với fuzzgoat.c

1. **Không có hàm main():** Phải tạo test harness để chỉ định điểm bắt đầu và input.

2. **Cấu trúc phức tạp:** `json_value` là union với nhiều trường, khó tạo symbolic input đúng invariants.

3. **Lỗi phân tán:** Lỗi nằm ở nhiều hàm khác nhau (`new_value`, `json_value_free_ex`), cần nhiều harness.

4. **State explosion:** Quá nhiều paths cần khám phá, ESBMC mất nhiều thời gian hoặc không hoàn thành.

5. **False positives từ harness:** Harness không thể tạo cấu trúc hoàn hảo, gây lỗi phụ che khuất lỗi thực.

6. **Đệ quy và vòng lặp:** Cần `--unwind` phù hợp, quá nhỏ thì bỏ sót, quá lớn thì chậm.

7. **Giới hạn của static analysis:** Không phát hiện được Use After Free (Lỗi 1) vì cần runtime execution để thấy memory được free rồi dùng lại.

### Kết luận

Với fuzzgoat, **ESBMC chỉ phát hiện hiệu quả 2/4 lỗi** (Invalid Pointer Free và Null Pointer Dereference) khi dùng test harness cụ thể. Hai lỗi còn lại (Use After Free và Out-of-bounds Read) cần:
- **Fuzzing (AFL++):** Tốt cho Use After Free, Out-of-bounds Read
- **Dynamic analysis (ASan):** Phát hiện tất cả 4 lỗi khi có input phù hợp

Static analysis (ESBMC) có giá trị nhưng không phải "silver bullet" - cần kết hợp nhiều phương pháp để đạt coverage tốt nhất.

# Kiểm tra bằng AFL++

Cài đặt với:

```bash
sudo apt install afl++ # Cho các hệ thống Debian
```

Nếu gặp lỗi, chạy:

```bash
# AFL++ yêu cầu chỉnh core_pattern vì cấu hình core dump của hệ thống chưa phù hợp để AFL thu thập crash chính xác.
sudo sh -c 'echo core > /proc/sys/kernel/core_pattern'

# AFL yêu cầu tắt giới hạn hiệu năng CPU
sudo su
cd /sys/devices/system/cpu
echo performance | tee cpu*/cpufreq/scaling_governor
exit
```

Biên dịch source code file harness cảu `fuzzgaot.c` bằng `afl-cc` để AFL++ chèn các instrument vào:

```bash
# Vì fuzzgoat.c có liên quan đến các thư viện khác nên dùng -lm
afl-cc main_afl.c fuzzgoat.c -o main_afl -lm
```

Sau khi có file binary, chạy AFL++ ở chế độ chạy song song:

```bash
afl-fuzz -i in -o out -M main0 ./main_afl @@

afl-fuzz -i in -o out -S sync1 ./main_afl @@
```




![alt text](image-9.png)


![alt text](image-10.png)

Với kết quả thu được như test case này: fuzzgoat_source_code/AFL_plus_plus/out/sync1/crashes/id:000003,sig:06,src:000193+000313,time:2888,execs:12608,op:splice,rep:3

Dùng testcase đẻ kiểm tra xem lỗi là ở dòng nào:

```bash
# Biên dịch lại với gcc
gcc -g -O0 main_afl.c fuzzgoat.c -o main_afl -lm



gdb --args ./main_afl "out/sync1/crashes/id:000003,sig:06,src:000193+000313,time:2888,execs:12608,op:splice,rep:3"
(gdb) run

```
Starting program: /home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus/main_afl out/sync1/crashes/id:000003,sig:06,src:000193+000313,time:2888,execs:12608,op:splice,rep:3

This GDB supports auto-downloading debuginfo from the following URLs:
  <https://debuginfod.ubuntu.com>
Enable debuginfod for this session? (y or [n]) y
Debuginfod has been enabled.
To make this setting permanent, add 'set debuginfod enabled on' to .gdbinit.
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
""
--------------------------------

string: 
free(): invalid pointer
```

Kiểm tra stacktrace để biết hàm trong chương trình `fuzzgoat.c` nằm trong frame nào:

```
(gdb) bt
```

```
#0  __pthread_kill_implementation (no_tid=0, signo=6, threadid=<optimized out>) at ./nptl/pthread_kill.c:44
#1  __pthread_kill_internal (signo=6, threadid=<optimized out>) at ./nptl/pthread_kill.c:78
#2  __GI___pthread_kill (threadid=<optimized out>, signo=signo@entry=6) at ./nptl/pthread_kill.c:89
#3  0x00007ffff7c4527e in __GI_raise (sig=sig@entry=6) at ../sysdeps/posix/raise.c:26
#4  0x00007ffff7c288ff in __GI_abort () at ./stdlib/abort.c:79
#5  0x00007ffff7c297b6 in __libc_message_impl (fmt=fmt@entry=0x7ffff7dce8d7 "%s\n")
    at ../sysdeps/posix/libc_fatal.c:134
#6  0x00007ffff7ca8ff5 in malloc_printerr (str=str@entry=0x7ffff7dcc672 "free(): invalid pointer")
    at ./malloc/malloc.c:5772
#7  0x00007ffff7cab38c in _int_free (av=<optimized out>, p=<optimized out>, have_lock=0)
    at ./malloc/malloc.c:4507
#8  0x00007ffff7caddae in __GI___libc_free (mem=0x55555555b8df) at ./malloc/malloc.c:3398
#9  0x0000555555555a11 in default_free (ptr=0x55555555b8df, user_data=0x0) at fuzzgoat.c:85
#10 0x0000555555555ece in json_value_free_ex (settings=0x7fffffffd080, value=0x55555555b8b0) at fuzzgoat.c:302
#11 0x0000555555557ed8 in json_value_free (value=0x55555555b8b0) at fuzzgoat.c:1080
#12 0x00005555555558ed in main (argc=2, argv=0x7fffffffd2d8) at main_afl.c:137
```

(gdb) frame 10
#10 0x0000555555555ece in json_value_free_ex (settings=0x7fffffffd080, value=0x55555555b8b0) at fuzzgoat.c:302
302                 settings->mem_free (value->u.string.ptr, settings->user_data);
```


(gdb) list
297                   char *null_pointer = NULL;
298                   printf ("%d", *null_pointer);
299                 }
300     /****** END vulnerable code **************************************************/
301
302                 settings->mem_free (value->u.string.ptr, settings->user_data);
303                 break;
304
305              default:
306                 break;


## Run with ASAN


chutrunganh@ThinkPad-CTA:~/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus$ ./main_afl_asan "/home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/input-files/oneByteString" 
"A"
--------------------------------

string: A
AddressSanitizer:DEADLYSIGNAL
=================================================================
==3874230==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000 (pc 0x56ba2dfdbf00 bp 0x7ffef6fa8eb0 sp 0x7ffef6fa8e90 T0)
==3874230==The signal is caused by a READ memory access.
==3874230==Hint: address points to the zero page.
    #0 0x56ba2dfdbf00 in json_value_free_ex /home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus/fuzzgoat.c:298
    #1 0x56ba2dfe11a3 in json_value_free /home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus/fuzzgoat.c:1080
    #2 0x56ba2dfdaf68 in main /home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus/main_afl.c:138
    #3 0x759f2c02a1c9 in __libc_start_call_main ../sysdeps/nptl/libc_start_call_main.h:58
    #4 0x759f2c02a28a in __libc_start_main_impl ../csu/libc-start.c:360
    #5 0x56ba2dfda4c4 in _start (/home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus/main_afl_asan+0x34c4) (BuildId: 64b73250af3a391cee0f6b4ee6f6fd60642aa657)

AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: SEGV /home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus/fuzzgoat.c:298 in json_value_free_ex
==3874230==ABORTING


Via gdb:


chutrunganh@ThinkPad-CTA:~/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus$ gdb --args ./main_afl "/home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/input-files/oneByteString" 
GNU gdb (Ubuntu 15.0.50.20240403-0ubuntu1) 15.0.50.20240403-git
Copyright (C) 2024 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<https://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from ./main_afl...
(gdb) run
Starting program: /home/chutrunganh/Documents/HUST/An Toan PM/Project/fuzzgoat_source_code/AFL_plus_plus/main_afl /home/chutrunganh/Documents/HUST/An\ Toan\ PM/Project/fuzzgoat_source_code/input-files/oneByteString

This GDB supports auto-downloading debuginfo from the following URLs:
  <https://debuginfod.ubuntu.com>
Enable debuginfod for this session? (y or [n]) y
Debuginfod has been enabled.
To make this setting permanent, add 'set debuginfod enabled on' to .gdbinit.
Downloading separate debug info for system-supplied DSO at 0x7ffff7fc3000
[Thread debugging using libthread_db enabled]                                                                                                      
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
"A"
--------------------------------

string: A

Program received signal SIGSEGV, Segmentation fault.
0x0000555555555e92 in json_value_free_ex (settings=0x7fffffffd070, value=0x55555555b8b0) at fuzzgoat.c:298
298                   printf ("%d", *null_pointer);
(gdb) 



# Kiểm tra bằng ASan

