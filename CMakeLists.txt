cmake_minimum_required(VERSION 3.12)
project(ftxui_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(deps/FTXUI)

add_executable(main main.cc)
target_link_libraries(main PRIVATE ftxui)
