#include <assert.h>

int main() {
    int x = 5;
    if (x > 0) {
        x = x + 1;
    }
    // Chúng ta mong muốn x phải bằng 10 (thực tế code trên x = 6, nên sẽ có lỗi)
    assert(x == 10); 
    return 0;
}