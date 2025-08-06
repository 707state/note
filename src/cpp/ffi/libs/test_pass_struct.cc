#include <iostream>
struct MyStruct {
    int x;
    int y;
    MyStruct() = default;
    MyStruct(int a, int b) : x(a), y(b) {}
};

extern "C" {
    MyStruct create_struct(int a, int b) {
        std::cout << "Now creating\n";
        return MyStruct(a, b);
    }
    void destroy_struct(void *struct_ptr) {
        std::cout << "Now destroying\n";
        delete static_cast<MyStruct *>(struct_ptr);
    }
    void print_struct(MyStruct arg) {
        std::cout << "Now printing\n";
        std::cout << "MyStruct: " << arg.x << ", " << arg.y << '\n';
    }
}
