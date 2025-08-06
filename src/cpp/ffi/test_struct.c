#include <ffi/ffi.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {
    int x;
    int y;
} MyStruct;

int main() {
    void *lib = dlopen("libtest_struct.dylib", RTLD_LAZY);
    if (!lib) {
        printf("Failed to load lib!");
        exit(1);
    }
    // 2. 获取函数指针
    typedef MyStruct* (*create_struct_t)(int, int);
    create_struct_t create_struct =
        (create_struct_t)dlsym(lib, "create_struct");
    typedef void (*print_struct_t)(void *);
    print_struct_t print_struct = (print_struct_t)dlsym(lib, "print_struct");
    typedef void (*destroy_struct_t)(void *);
    destroy_struct_t destroy_struct =
        (destroy_struct_t)dlsym(lib, "destroy_struct");

    // 3. 准备 libffi 调用
    ffi_cif cif;
    ffi_type *args[] = { &ffi_type_sint32, &ffi_type_sint32 };
    ffi_type *ret_type = &ffi_type_pointer;
    int a = 1, b = 2;
    void *values[] = {&a, &b};
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, ret_type, args) != FFI_OK) {
        fprintf(stderr, "ffi_prep_cif failed\n");
        dlclose(lib);
        return 1;
    }
    MyStruct *result;
    ffi_call(&cif, (void (*)(void))create_struct, &result, values);
    MyStruct *struct_result = (MyStruct *)result;

    ffi_cif cif2;
    ffi_type *print_args[] = {&ffi_type_pointer};
    void *print_values[] = {&result};
    if (ffi_prep_cif(&cif2, FFI_DEFAULT_ABI, 1, &ffi_type_void, print_args) != FFI_OK) {
        fprintf(stderr, "ffi_prep_cif2 failed\n");
        dlclose(lib);
        return 1;
    }
    ffi_call(&cif2, print_struct, NULL, print_values);
    ffi_cif cif3;
    ffi_type *destroy_args[] = {&ffi_type_pointer};
    void *destroy_values[] = {&result};
    if (ffi_prep_cif(&cif3, FFI_DEFAULT_ABI, 1, &ffi_type_void, print_args) != FFI_OK) {
        fprintf(stderr, "ffi_prep_cif3 failed\n");
        dlclose(lib);
        return 1;
    }
    ffi_call(&cif3,destroy_struct,NULL,destroy_values);
}
