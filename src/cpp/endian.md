# C/C++判断大小端序

```c++
#include <iostream>
#include <cstring>

bool isLittleEndian() {
    int i = 1;
    char c;
    std::memcpy(&c, &i, 1);
    return (c == 1);
}

int main() {
    if (isLittleEndian()) {
        std::cout << "系统是小端序" << std::endl;
    } else {
        std::cout << "系统是大端序" << std::endl;
    }
    return 0;
}
```
