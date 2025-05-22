#include <iostream>
extern "C"
void test_hello(char *args) {
    std::cout<<"This is test_hello: "<<args;
}
