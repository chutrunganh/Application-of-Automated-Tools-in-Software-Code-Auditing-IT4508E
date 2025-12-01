#include <assert.h>

int divide(int a, int b) {
    // Tiềm ẩn lỗi chia cho 0
    return a / b;
}

int main() {
    int a = nondet_int();  // ← CHỈ ĐỊNH: a là symbolic
    int b = nondet_int();   // ← CHỈ ĐỊNH: b là symbolic
    int result = divide(a, b);
}