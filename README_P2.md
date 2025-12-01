# Tổng quan chương trình fuzzgoat

**Fuzzgoat** là một chương trình C mã nguồn mở, được sửa đổi từ thư viện `udp/json-parser`. Chức năng chính của nó là đọc một tệp JSON từ đầu vào chuẩn (stdin) hoặc từ tệp, phân tích cú pháp và in ra cấu trúc dữ liệu tương ứng. 

Cụ thể chương trình hoạt động theo mô hình đệ quy (recursive descent parser). Nó đọc một chuỗi byte đầu vào và cố gắng xây dựng một cấu trúc dữ liệu cây (tree) đại diện cho đối tượng JSON.

- Input: File JSON hoặc luồng dữ liệu từ stdin.
- Process: Hàm json_parse gọi các hàm con để xử lý Object, Array, String, Number, Boolean, v.v.
- Output: Chương trình thường không in ra kết quả phân tích mà chỉ trả về mã thoát (exit code), trừ khi gặp lỗi crash do các lỗ hổng.


Cấu trúc thư mục:

```
fuzzgoat/
├── in/seed            # Chứa seed đầu vào cho AFL++ fuzzing
├── fuzzgoat.c         # Mã nguồn chính của chương trình
├── fuzzgoat.h         # Khai báo hàm cho fuzzgoat.c
├── main.c             # Hàm main để khởi động chương trình
├── fuzzgoatNoVulns.c  # Phiên bản không có lỗ hổng của fuzzgoat
└── Makefile           # Tập lệnh build
```

`main.c` là entry point khi chạy theo cách truyền thống (không fuzz), hoặc được dùng làm “harness” tối giản để AFL/LibFuzzer có điểm vào.

*Với fuzzers như AFL++, harness thường là một chương trình có main() đọc dữ liệu từ stdin hoặc @@ (đường dẫn file do AFL cấp) rồi chuyển dữ liệu đó vào hàm bạn muốn fuzz.*

Biên dịch:

```bash
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

- Vị trí: Hàm `new_value`, bên trong `case json_array`.

```c
if (value->u.array.length == 0)
{
   free(*top); // Dòng gây lỗi
   break;
}
```

- Phân tích nguyên nhân: Khi trình phân tích cú pháp (parser) gặp một mảng JSON rỗng (`[]`), nó thực hiện lệnh `free(*top)` để giải phóng khối nhớ được trỏ bởi `*top`. Tuy nhiên, con trỏ này không được gán lại thành `NULL` hoặc được xử lý đúng cách để ngăn chặn việc truy cập sau đó. Chương trình vẫn tiếp tục chạy và cố gắng sử dụng vùng nhớ đã bị giải phóng này ở các bước tiếp theo.

Ý đồ là giải phóng con trỏ `*top` khi mảng rỗng vì mảng khi này không cần thiết. Tuy nhiên, con trỏ `*top` này sau đó vẫn được tham chiếu hoặc sử dụng trong logic dọn dẹp bộ nhớ (`json_value_free`).


- Hậu quả: Gây ra lỗi hư hỏng bộ nhớ (memory corruption), có thể dẫn đến crash chương trình hoặc trong các tình huống thực tế nghiêm trọng hơn là thực thi mã tùy ý (arbitrary code execution).

- Cách kích hoạt: Sử dụng input là một mảng JSON rỗng: 
  - Payload: `[]`
  - File mẫu: `input-files/emptyArray`

**2. Lỗ hổng Out-of-bounds Read / Invalid Free (Đọc ngoài vùng nhớ / Giải phóng sai)**

- Vị trí: Hàm `json_value_free_ex`, bên trong case `json_object`

```c
value = value->u.object.values [value->u.object.length--].value;
```

- Phân tích nguyên nhân: Đoạn mã sử dụng toán tử giảm sau (post-decrement) `length--` làm chỉ số mảng.

  - Nếu mảng có độ dài là `N`, các chỉ số hợp lệ là từ `0` đến `N-1`.

  - Việc sử dụng `[length--]` sẽ truy cập vào phần tử tại chỉ số `N` (vượt quá giới hạn mảng), sau đó mới giảm giá trị `length`. Điều này dẫn đến việc đọc dữ liệu rác hoặc dữ liệu không thuộc quyền quản lý của mảng đó.

- Hậu quả: Đọc bộ nhớ không hợp lệ, dẫn đến việc chương trình cố gắng giải phóng (free) một con trỏ rác hoặc sai địa chỉ trong câu lệnh if phía trên logic này, gây crash hoặc tham chiếu sai bộ nhớ.

- Cách kích hoạt: Sử dụng một đối tượng JSON hợp lệ bất kỳ.

  - Payload: `{"":0}`

  - File mẫu: `input-files/validObject`.

- Hậu quả: `free()` yêu cầu con trỏ phải trỏ chính xác vào đầu vùng nhớ được cấp phát bởi `malloc()`. Việc truyền một con trỏ sai lệch (trỏ vào redzone hoặc metadata của allocator) sẽ gây ra lỗi Invalid Free hoặc làm hỏng cấu trúc heap (heap corruption).



**3. Lỗ hổng Invalid Pointer Free (Giải phóng con trỏ không hợp lệ)**

- Vị trí: Hàm `json_value_free_ex`, bên trong case `json_string`.

```c
if (!value->u.string.length){
  value->u.string.ptr--; // Dòng gây lỗi
}
// ... sau đó ...
settings->mem_free (value->u.string.ptr, settings->user_data);
```

- Phân tích nguyên nhân: Nếu chuỗi JSON là chuỗi rỗng (độ dài bằng 0), mã nguồn cố tình giảm địa chỉ con trỏ `value->u.string.ptr` đi 1 đơn vị. Sau đó, chương trình gọi hàm `mem_free` (tương đương `free`) lên con trỏ đã bị thay đổi này. Trình quản lý bộ nhớ chỉ có thể giải phóng địa chỉ bắt đầu chính xác của khối nhớ đã cấp phát; việc truyền vào một địa chỉ sai sẽ gây lỗi.

- Hậu quả: Gây lỗi phân bổ bộ nhớ, thường dẫn đến `SIGABRT` (Process abort signal) hoặc crash chương trình ngay lập tức.

- Cách kích hoạt: Sử dụng input là một chuỗi JSON rỗng.

  - Payload: `""`

  - File mẫu: `input-files/emptyString.`

**4. Lỗ hổng Null Pointer Dereference (Truy cập con trỏ NULL)**
- Vị trí: hàm `json_value_free_ex`, bên trong case `json_string`.


```c
if (value->u.string.length == 1) {
  char *null_pointer = NULL;
  printf ("%d", *null_pointer); // Dòng gây lỗi
}
```
- Phân tích nguyên nhân: Đoạn mã kiểm tra nếu chuỗi có độ dài bằng 1. Nếu đúng, nó khởi tạo một con trỏ `null_pointer` với giá trị `NULL` và cố gắng truy cập (dereference) giá trị mà nó trỏ tới để in ra.

- Hậu quả: Truy cập vào địa chỉ 0 (NULL) là bất hợp pháp trong hầu hết các hệ điều hành hiện đại, dẫn đến việc hệ điều hành chấm dứt chương trình ngay lập tức (Segmentation Fault / SIGSEGV).

- Cách kích hoạt: Sử dụng input là một chuỗi JSON có độ dài đúng bằng 1 ký tự.

  - Payload: `"A"`

  - File mẫu: `input-files/oneByteString`.

# Kiểm tra bằng ESBMC

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




# Kiểm tra bằng AFL++

afl-fuzz -i in -o out ./fuzzgoat @@



# Kiểm tra bằng ASan

