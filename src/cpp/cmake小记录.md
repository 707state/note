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
