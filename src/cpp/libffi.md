# C库libffi
核心要点：

## 数据结构
- ffi_cif: 用来描述调用约定。
- ffi_type: 描述数据结构。

## 函数
- ffi\_prep\_cif: 初始化ffi_cif结构体。
- ffi\_call: 调用Native函数。

# 举例

```c++
int main() {
    ffi_cif cif;
    ffi_type *args[1];
    void *values[1];
    char *s;
    ffi_arg rc;
    /* Initialize the argument info vectors */
    args[0] = &ffi_type_pointer;
    values[0] = &s;
    /* Initialize the cif */
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint, args) == FFI_OK) {
        s = "Hello World!";
        ffi_call(&cif, puts, &rc, values);
        /* rc now holds the result of the call to puts */
        /* values holds a pointer to the function’s arg, so to
           call puts() again all we need to do is change the
    value of s */
        s = "This is cool!";
        ffi_call(&cif, puts, &rc, values);
    }
    return 0;
}
```

这里调用的函数是puts，代码中没有显式调用，puts而是用ffi的方式去调用puts函数。

## 结构体

libffi允许结构体作为参数的传递（返回值或者函数参数）。

核心是一个ffi_type。

```c++
    ffi_type ffi_type_my_struct = {
        .type = FFI_TYPE_STRUCT,
        .size = sizeof(MyStruct),
        .alignment = alignof(MyStruct),
        .elements = placeholder};
```

这之后这个struct就可以和别的ffi类型一样用了。注意，这里涉及到elements数组的生命周期，可以做封装来利用C++的RAII。


# 要点

- 一个cif对应一个FFI函数的上下文，也就是说不能绑定多个。

- values（也就是通过ffi\_call传递的形参），要通过取地址的方式传入。
