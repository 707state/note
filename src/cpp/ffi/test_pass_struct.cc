#include <dlfcn.h>
#include <ffi/ffi.h>
#include <stdalign.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#define FFI_TYPE_OF(T, ...)                                                    \
  static ffi_type ffi_type_##T = {.size = sizeof(T),                           \
                                  .alignment = alignof(T),                     \
                                  .type = FFI_TYPE_STRUCT,                     \
                                  .elements =                                  \
                                      (ffi_type *[]){__VA_ARGS__, NULL}};

struct MyStruct {
  int x;
  int y;
};
template <typename T> struct FFITypeMapper {
  static constexpr ffi_type *value = nullptr;
};
template <> struct FFITypeMapper<char> {
  static constexpr ffi_type *value = &ffi_type_schar;
};
template <> struct FFITypeMapper<short> {
  static constexpr ffi_type *value = &ffi_type_sshort;
};
template <> struct FFITypeMapper<int> {
  static constexpr ffi_type *value = &ffi_type_sint32;
};
template <> struct FFITypeMapper<float> {
  static constexpr ffi_type *value = &ffi_type_float;
};
template <> struct FFITypeMapper<double> {
  static constexpr ffi_type *value = &ffi_type_double;
};
template <> struct FFITypeMapper<long> {
  static constexpr ffi_type *value = &ffi_type_slong;
};
template <> struct FFITypeMapper<long long> {
  static constexpr ffi_type *value = &ffi_type_sint64;
};
template <> struct FFITypeMapper<unsigned char> {
  static constexpr ffi_type *value = &ffi_type_uchar;
};
template <> struct FFITypeMapper<unsigned short> {
  static constexpr ffi_type *value = &ffi_type_ushort;
};
template <> struct FFITypeMapper<unsigned int> {
  static constexpr ffi_type *value = &ffi_type_uint32;
};
template <> struct FFITypeMapper<unsigned long> {
  static constexpr ffi_type *value = &ffi_type_ulong;
};
template <> struct FFITypeMapper<unsigned long long> {
  static constexpr ffi_type *value = &ffi_type_uint64;
};
template <> struct FFITypeMapper<bool> {
  static constexpr ffi_type *value = &ffi_type_uint8;
};
template <> struct FFITypeMapper<void> {
  static constexpr ffi_type *value = &ffi_type_void;
};
template <typename T> struct FFITypeMapper<T *> {
  static constexpr auto value = &ffi_type_pointer;
};
template <typename T> class FFIStructType {
public:
    FFIStructType():struct_size(sizeof(T)),struct_alignment(alignof(T)) {
    }
    template <typename... FieldTypes>
    void setElements(FieldTypes T::*...value) {
        elements = new ffi_type *[] { FFITypeMapper<FieldTypes>::value..., NULL };
    }
    ffi_type getFFIType() const {
        return {
            .type = FFI_TYPE_STRUCT, .size = struct_size,
            .alignment = struct_alignment,
            .elements = elements};
    }
    ~FFIStructType() {
        delete[] elements;
    }
private:
    ffi_type **elements;
    size_t struct_size;
    unsigned short struct_alignment;
    };
int main() {
    void *lib = dlopen("libtest_pass_struct.dylib", RTLD_LAZY);
    if (!lib) {
        printf("Failed to load lib!");
        exit(1);
    }
    // 2. 获取函数指针
    typedef MyStruct (*create_struct_t)(int, int);
    create_struct_t create_struct = (create_struct_t)dlsym(lib, "create_struct");
    typedef void (*print_struct_t)(void *);
    print_struct_t print_struct = (print_struct_t)dlsym(lib, "print_struct");
    typedef void (*destroy_struct_t)(void *);
    destroy_struct_t destroy_struct =
        (destroy_struct_t)dlsym(lib, "destroy_struct");
    // 3. 准备 libffi 调用
    // 创建一个MyStruct的ffi_type
    ffi_cif cif;
    ffi_type *args[] = {&ffi_type_sint32, &ffi_type_sint32};
    FFIStructType<MyStruct> my_type;
    my_type.setElements(&MyStruct::x, &MyStruct::y);
    ffi_type ret_type = my_type.getFFIType();
    int a = 1, b = 2;
    void *values[] = {&a, &b};
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ret_type, args) != FFI_OK) {
        fprintf(stderr, "ffi_prep_cif failed\n");
        dlclose(lib);
        return 1;
    }
    MyStruct result;
    ffi_call(&cif, (void (*)(void))create_struct, &result, values);

    ffi_cif cif2;
    ffi_type *print_args[] = {&ffi_type_pointer};
    void *print_values[] = {&result};
    if (ffi_prep_cif(&cif2, FFI_DEFAULT_ABI, 1, &ffi_type_void, print_args) !=
        FFI_OK) {
        fprintf(stderr, "ffi_prep_cif2 failed\n");
        dlclose(lib);
        return 1;
    }
    ffi_call(&cif2, (void (*)(void))print_struct, NULL, print_values);
}
