#include "ffi/ffi.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
int main() {
    void *lib_test_hello = dlopen("libtest_hello.dylib", RTLD_LAZY);
  void (*test_hello_ptr)(char *) =
      (void (*)(char *))dlsym(lib_test_hello, "test_hello");
  if (!test_hello_ptr) {
    fprintf(stderr, "Error: %s\n", dlerror());
    dlclose(lib_test_hello);
    exit(1);
  }
  ffi_cif cif;
  ffi_type *args[1];
  void *values[1];
  char *s;
  ffi_arg rc;
  ffi_status status =
      ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args);
  // 4. 准备参数
  const char* message = "Hello from libffi!";
  values[0] = &message;
  /* Initialize the argument info vectors */
  args[0] = &ffi_type_pointer;
  values[0] = &s;
  /* Initialize the cif */
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint, args) == FFI_OK) {
      s = "Hello World!";
      ffi_call(&cif, (void (*)(void))test_hello_ptr, &rc, values);
      /* rc now holds the result of the call to puts */
      /* values holds a pointer to the function’s arg, so to
         call puts() again all we need to do is change the
    value of s */
  }
  return 0;
}
