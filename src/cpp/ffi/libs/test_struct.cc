#include <iostream>

extern "C" {
    struct MyStruct {
        int x;
        int y;
        MyStruct() = default;
        MyStruct(int a, int b) : x(a), y(b) {}
    };
    void *create_struct(int a, int b) {
        std::cout<<"Now creating\n";
        return new MyStruct(a, b); }
    void destroy_struct(void *struct_ptr) {
        std::cout<<"Now destroying\n";
        delete static_cast<MyStruct *>(struct_ptr);
    }
    void print_struct(void *arg) {
        std::cout << "Now printing\n";
        MyStruct* args=static_cast<MyStruct *>(arg);
        std::cout<<"MyStruct: "<<args->x<<", "<<args->y<<'\n';
    }
}
