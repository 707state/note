# 构建系统碎碎念
C++的构建系统，没10个也有5个了，de factor standard 的cmake，社区常见的meson/xmake，谷歌主推的bazel，腾讯自家的blade，facebook搞的buck/buck2，还有古老的gnu automake，就连zig和gradle也能过来凑一桌。

还是cmake吧，至少资料多，遇到问题好搜。吗？

## cmake的误解

cmake实际上不是一个单纯的DSL，而是一个图灵完备的脚本语言。

比如说，cmake可以调用shell，可以设置环境变量，可以用来替代很多python/js脚本，不过最本质的用途还是c/c++的项目构建。

常用的cmake命令有add\_subdirectory，include，add\_executable等等，大多数人会停留在能用就行的阶段，但是，我不这么认为！

cmake许多核心的能力切切实实的让我体会到了便捷，有必要去深入了解一下。

## 函数参数解析

cmake有一个不太常用的东西叫做cmake\_parse\_arguments，这个函数非常强大，可以说cmake的许多函数的实现都是靠这个函数。

在 CMake 中，宏或函数的参数默认是位置参数列表，这意味着你只能通过 ${ARGV0}、${ARGV1} 或 ARGN 等方式获取，非常不便。

而 cmake_parse_arguments 提供了一种机制，让你可以指定哪些是选项（布尔值）、哪些是单值参数、哪些是多值参数，从而支持如下格式的参数：

```cmake
my_macro(
  OPTION1             # 选项（布尔）
  NAME my_name        # 单值参数
  SRCS a.cpp b.cpp    # 多值参数
)
```

语法：
```cmake
cmake_parse_arguments(
  <prefix>
  "<options>"
  "<one_value_keywords>"
  "<multi_value_keywords>"
  <args>...
)
```

<prefix>：解析后的变量名前缀，会自动生成类似 ${<prefix>_OPTION}、${<prefix>_NAME}、${<prefix>_SRCS} 等变量。
<options>：布尔开关选项列表（无需值）
<one_value_keywords>：期望一个值的参数（如 NAME）
<multi_value_keywords>：可以跟多个值的参数（如 SRCS）
<args>：实际传入的参数，可以是 ${ARGV} 或 ${ARGN}。

用这个函数可以做到一些很方便的操作，比如说避免条件编译而是把编译放在单独的编译选项中，在编译时把符合要求的实现加入进来而不是通过大量的宏来隐藏实现细节。

### 例子

```cmake
function(auto_link TARGET)
    cmake_parse_arguments(ARG
        "EXE;LIB"
        ""
        "LIBS;INCLUDES"
        ${ARGN})
    set(SOURCES ${ARG_UNPARSED_ARGUMENTS})
    if(ARG_EXE)
        add_executable(${TARGET} ${SOURCES})
    elseif(ARG_LIB)
        add_library(${TARGET} ${SOURCES})
    endif()
    target_link_libraries(${TARGET} PUBLIC ${ARG_LIBS})
    target_link_libraries(${TARGET}
    PUBLIC
    ${ARG_INCLUDES})
    set_target_properties(${TARGET}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
endfunction()
```
这个函数可以简化添加一个target所需要的那些命令，对我而言非常方便。

# aux_source_directory
把一个路径下的所有源文件添加到一个列表里面，可以用来简化添加library或者executable的过程。

# 生成器表达式
作用：在CMake生成构建系统的时候根据不同配置动态生成特定的内容。

可以在条件编译或者条件定义的情况下提供不同的配置项。

## 变量查询
在实际使用的时候会根据不同CMake内置变量生成不同配置，核心就在于“判断”：

- $<TARGET_EXISTS:target>：判断目标是否存在
- $<CONFIG:cfgs>：判断编译类型配置是否包含在cfgs列表（比如"release,debug"）中；不区分大小写
- $<PLATFORM_ID:platform_ids>：判断CMake定义的平台ID是否包含在platform_ids列表中
- $<COMPILE_LANGUAGE:languages>：判断编译语言是否包含在languages列表中

## 字符串生成器表达式
例如：基于编译器ID指定include目录：
```cmake
include_directories(/usr/include/$<CXX_COMPILER_ID>/)
```
根据编译器的类型，$<CXX_COMPILER_ID>会被替换成对应的ID（比如“GNU”、“Clang”）。

## 条件表达式

主要有两个格式：

1. $<condition:true_string>：如果条件为真，则结果为true_string，否则为空
2. $<IF:condition,str1,str2>：如果条件为真，则结果为str1，否则为str2

适用于简化编译选项：

```cmake
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
```

通过应用生成器表达式可以变为：

```cmake
add_compile_options("$<$<CONFIG:Debug>:-g;-O0>")
add_compile_options($<$<CONFIG:Release>:-O2>)
```

# cmake常用option总结

1. CMAKE\_BUILD\_TYPE：设置优化类型。
2. CMAKE\_EXPORT\_COMPILE_COMMANDS：导出编译数据库。
3. BUILD\_SHARED\_LIBS: 觉得动态/静态链接。
4. CMAKE\_C\_COMPILER和CMAKE\_CXX\_COMPILER：设置编译器。
