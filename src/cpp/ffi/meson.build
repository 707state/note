project('ffi_study', 'c', 'cpp')

# 依赖
ffi_dep = dependency('libffi')

# 库目标
shared_library('test_hello', 'libs/test_hello.cc', cpp_args: '-fPIC')
shared_library('test_struct', 'libs/test_struct.cc', cpp_args: '-fPIC')

# 可执行文件目标
executable('test_ffi', 'test.c',
  dependencies: ffi_dep,
  c_args: '-Wno-incompatible-function-pointer-types'
)

executable('test_ffi_struct', 'test_struct.c',
  dependencies: ffi_dep, 
  c_args: '-Wno-incompatible-function-pointer-types'
)


