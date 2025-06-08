cmake_minimum_required(VERSION 3.22)
project(ffi_study)

function(auto_link TARGET)
  cmake_parse_arguments(ARG
    "LIB;EXE"
    ""
    "LIBS;LINK;COMPILE" ${ARGN})
  set(SOURCES ${ARG_UNPARSED_ARGUMENTS})
  if(ARG_EXE)
    add_executable(${TARGET} ${SOURCES})
  elseif(ARG_LIB)
    add_library(${TARGET} ${SOURCES})
  else()
    message(FATAL_ERROR "Source file must be one of executable or library")
  endif()
  if(ARG_LIBS)
    target_link_libraries(${TARGET} PUBLIC ${ARG_LIBS})
  endif()
  if(ARG_LINK)
    target_link_options(${TARGET} PRIVATE ${ARG_LINK})
  endif()
  if(ARG_COMPILE)
    target_compile_options(${TARGET} PRIVATE ${ARG_COMPILE})
  endif()
endfunction()
auto_link(test_hello
  libs/test_hello.cc
  LIB
  COMPILE -fPIC
)
auto_link(test_struct
  libs/test_struct.cc
  LIB
  COMPILE -fPIC
)
auto_link(test_pass_struct
  libs/test_pass_struct.cc
  LIB
  COMPILE -fPIC)
auto_link(test_ffi
  test.c
  EXE
  LINK -lffi
  COMPILE -Wno-incompatible-function-pointer-types
)
auto_link(test_ffi_struct
  test_struct.c
  EXE
  LINK -lffi
  COMPILE -Wno-incompatible-function-pointer-types
)
auto_link(test_ffi_pass_struct
  test_pass_struct.cc
  EXE
  LINK -lffi
  COMPILE -Wno-incompatible-function-pointer-types)
