# TẠI SAO CẦN NHIỀU TEST HARNESS?

## ❓ CÂU HỎI

"Đã có `test_esbmc_focused.c` rồi, tại sao còn cần `test_string_empty.c` và `test_string_one.c`?"

## ✅ CÂU TRẢ LỜI NGẮN GỌN

**Vì ESBMC dừng lại khi phát hiện lỗi đầu tiên!**

- `test_esbmc_focused.c` → Phát hiện lỗi Object (dòng 258) → DỪNG
- `test_string_empty.c` → Phát hiện lỗi Empty String (dòng 279) → DỪNG  
- `test_string_one.c` → Phát hiện lỗi One-byte String (dòng 298) → DỪNG

## 📊 SO SÁNH CHI TIẾT

### Test 1: test_esbmc_focused.c (Tổng quát)

```c
int type_choice = nondet_int();  // Type có thể là 0-7
__ESBMC_assume(type_choice >= 0 && type_choice <= 7);

switch (value->type) {
    case json_array: { ... }
    case json_object: { ... }  // ← ESBMC phát hiện lỗi ở đây TRƯỚC
    case json_string: { ... }  // ← Chưa kịp khám phá đến đây
}
```

**Kết quả:**
- ✅ Phát hiện lỗi Object (dòng 258)
- ❌ KHÔNG phát hiện lỗi Empty String (dòng 279)
- ❌ KHÔNG phát hiện lỗi One-byte String (dòng 298)

**Lý do:** ESBMC khám phá `type=1` (json_object) trước, phát hiện lỗi, và **DỪNG LẠI**.

### Test 2: test_string_empty.c (Cụ thể)

```c
value->type = json_string;  // ← CHỈ ĐỊNH: type = string
value->u.string.length = 0;  // ← CHỈ ĐỊNH: length = 0
```

**Kết quả:**
- ❌ KHÔNG phát hiện lỗi Object (vì type = string)
- ✅ Phát hiện lỗi Empty String (dòng 279)
- ❌ KHÔNG phát hiện lỗi One-byte String (vì length = 0, không phải 1)

**Lý do:** ESBMC tập trung vào path `type=string, length=0`, phát hiện lỗi, và **DỪNG LẠI**.

### Test 3: test_string_one.c (Cụ thể)

```c
value->type = json_string;  // ← CHỈ ĐỊNH: type = string
value->u.string.length = 1;  // ← CHỈ ĐỊNH: length = 1
```

**Kết quả:**
- ❌ KHÔNG phát hiện lỗi Object (vì type = string)
- ❌ KHÔNG phát hiện lỗi Empty String (vì length = 1, không phải 0)
- ✅ Phát hiện lỗi One-byte String (dòng 298)

**Lý do:** ESBMC tập trung vào path `type=string, length=1`, phát hiện lỗi, và **DỪNG LẠI**.

## 🔍 TẠI SAO ESBMC DỪNG KHI PHÁT HIỆN LỖI ĐẦU TIÊN?

Đây là đặc điểm của **Bounded Model Checking (BMC)**:

1. **Mục tiêu**: Tìm xem có lỗi KHÔNG (có/không)
2. **Khi tìm thấy lỗi**: Dừng lại và báo `VERIFICATION FAILED`
3. **Không tiếp tục**: Không cần tìm thêm lỗi khác

**Ví dụ:**
```
ESBMC: "Tôi tìm thấy lỗi ở path type=1, length=4!"
      → DỪNG
      → Báo: VERIFICATION FAILED
      → KHÔNG tiếp tục tìm lỗi ở path type=5, length=0
```

## 💡 CÓ THỂ GỘP THÀNH 1 FILE KHÔNG?

### ❌ Cách 1: Gộp tất cả vào test_esbmc_focused.c

**Vấn đề:**
- ESBMC vẫn dừng khi phát hiện lỗi đầu tiên
- Vẫn chỉ phát hiện được 1 lỗi

### ✅ Cách 2: Giữ nhiều file (HIỆN TẠI)

**Ưu điểm:**
- Mỗi file tập trung vào một loại lỗi
- Dễ hiểu và debug
- Có thể chạy riêng từng test

**Nhược điểm:**
- Nhiều file hơn
- Cần chạy nhiều lần

### ✅ Cách 3: Sử dụng script tự động (KHUYẾN NGHỊ)

**Giải pháp:** Giữ nhiều file nhưng dùng script tự động:

```bash
./test_all_bugs.sh  # Tự động chạy tất cả
```

**Ưu điểm:**
- Tự động chạy tất cả tests
- Phát hiện tất cả lỗi
- Dễ sử dụng

## 📋 KẾT LUẬN

### Các file CẦN THIẾT:

1. ✅ **test_esbmc_focused.c** - Phát hiện lỗi Object
2. ✅ **test_string_empty.c** - Phát hiện lỗi Empty String
3. ✅ **test_string_one.c** - Phát hiện lỗi One-byte String
4. ✅ **test_all_bugs.sh** - Script tự động chạy tất cả

### Có thể xóa không?

**KHÔNG!** Vì:
- Mỗi file phát hiện một lỗi khác nhau
- ESBMC dừng khi phát hiện lỗi đầu tiên
- Cần nhiều file để phát hiện tất cả lỗi

### Cách sử dụng:

```bash
# Cách 1: Chạy từng test riêng
esbmc test_esbmc_focused.c fuzzgoat.c --memory-leak-check --unwind 10
esbmc test_string_empty.c fuzzgoat.c --memory-leak-check --unwind 5
esbmc test_string_one.c fuzzgoat.c --memory-leak-check --unwind 5

# Cách 2: Chạy script tự động (KHUYẾN NGHỊ)
./test_all_bugs.sh
```

## 🎯 TÓM TẮT

**Câu hỏi:** Có thể xóa `test_string_empty.c` và `test_string_one.c` không?

**Trả lời:** **KHÔNG!** Vì:
1. ESBMC dừng khi phát hiện lỗi đầu tiên
2. `test_esbmc_focused.c` chỉ phát hiện được lỗi Object
3. Cần các file riêng để phát hiện các lỗi khác
4. Script `test_all_bugs.sh` tự động chạy tất cả

**Giải pháp tốt nhất:** Giữ tất cả các file và dùng script tự động!

