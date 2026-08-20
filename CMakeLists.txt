cmake_minimum_required(VERSION 3.12)
project(ftxui_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(deps/FTXUI)
add_subdirectory(deps/eigen)

add_executable(main_3d_animation main.cc)
add_executable(main_3d_animation_with_eigen  main_eigen.cc)


target_link_libraries(main_3d_animation PRIVATE ftxui eigen)
target_link_libraries(main_3d_animation_with_eigen PRIVATE ftxui eigen)
