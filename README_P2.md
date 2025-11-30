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

**1 Lỗi Use-After-Free (UAF) với Mảng Rỗng**

- Vị trí: Hàm `new_value`, trong khối xử lý `json_array` (dòng 124)
- Cơ chế: Khi parser gặp một mảng rỗng `[]`, mã nguồn được chèn thêm một lệnh `free(*top)` :

```c
if (value->u.array.length == 0) {
    free(*top);
}
```

Ý đồ là giải phóng con trỏ `*top` khi mảng rỗng vì mảng khi này không cần thiết. Tuy nhiên, con trỏ `*top` này sau đó vẫn được tham chiếu hoặc sử dụng trong logic dọn dẹp bộ nhớ (`json_value_free`).


- Hậu quả: Chương trình truy cập vào vùng nhớ đã được trả lại cho hệ thống. Nếu vùng nhớ này đã bị ghi đè hoặc cấp phát cho đối tượng khác, hành vi của chương trình sẽ không xác định, có thể dẫn đến thực thi mã từ xa.


Dấu hiệu nhận biết với ASan: Báo cáo ERROR: AddressSanitizer: heap-use-after-free.

4.2 Lỗi Invalid Free do Con trỏ bị Dịch chuyển

Vị trí: Hàm json_value_free_ex, xử lý chuỗi rỗng ("").
Cơ chế: Một đoạn mã kiểm tra nếu chuỗi có độ dài bằng 0, nó sẽ thực hiện ptr-- (giảm con trỏ đi 1 byte). Sau đó, con trỏ bị dịch chuyển này được truyền vào hàm free().
Hậu quả: free() yêu cầu con trỏ phải trỏ chính xác vào đầu vùng nhớ được cấp phát bởi malloc. Việc truyền một con trỏ sai lệch (trỏ vào redzone hoặc metadata của allocator) sẽ gây ra lỗi Invalid Free hoặc làm hỏng cấu trúc heap (heap corruption).
Dấu hiệu nhận biết với ASan: Báo cáo attempting free on address which was not malloc()-ed.

4.3 Lỗi Null Pointer Dereference

Vị trí: Xử lý chuỗi có độ dài 1 ký tự (ví dụ: "A").
Cơ chế: Mã nguồn có đoạn kiểm tra: if (length == 1). Bên trong khối này, một con trỏ NULL được khởi tạo và chương trình cố gắng in giá trị của nó: printf("%d", *null_pointer).
Hậu quả: Truy cập địa chỉ 0 gây ra Segmentation Fault (Segfault) ngay lập tức, làm sập chương trình (Denial of Service).
Dấu hiệu nhận biết với ASan: Báo cáo SEGV on unknown address 0x000000000000.

4.4 Lỗi Invalid Read / Heap Corruption (Object)

Vị trí: Hàm json_value_free_ex, xử lý json_object.
Cơ chế: Một vòng lặp sử dụng value->u.object.length-- sai cách, dẫn đến việc chỉ số mảng bị giảm xuống mức âm hoặc truy cập phần tử mảng sai lệch trước khi giải phóng.
Hậu quả: Đọc/Ghi ngoài vùng nhớ heap (Heap Buffer Overflow/Underflow).



# Kiểm tra bằng ESBMC


# Kiểm tra bằng AFL++

afl-fuzz -i in -o out ./fuzzgoat @@



# Kiểm tra bằng ASan

