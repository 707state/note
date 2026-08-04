cmake_minimum_required(VERSION 3.24)

project(MyVulkanProject
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#
# Vulkan-Headers — 始终使用 externals 中的源代码
#
add_subdirectory(externals/Vulkan-Headers)


add_executable(
    MyVulkanApp
    src/main.cc
)



option(BUILD_VULKAN_LOADER "Build Vulkan-Loader from externals source" OFF)

if(BUILD_VULKAN_LOADER)
    #
    # 从 externals 源码构建 Vulkan-Loader
    #   便于调试 loader 内部逻辑
    #
    # 禁用所有 WSI 支持——只用 compute shader，不做窗口渲染
    set(BUILD_WSI_XCB_SUPPORT OFF CACHE BOOL "" FORCE)
    set(BUILD_WSI_XLIB_SUPPORT OFF CACHE BOOL "" FORCE)
    set(BUILD_WSI_XLIB_XRANDR_SUPPORT OFF CACHE BOOL "" FORCE)
    set(BUILD_WSI_WAYLAND_SUPPORT OFF CACHE BOOL "" FORCE)

    add_subdirectory(externals/Vulkan-Loader)

    target_link_libraries(
        MyVulkanApp
        PRIVATE
        Vulkan::Headers     # externals 中的头文件
        Vulkan::Loader      # 源码构建的 loader（内部已链接 Vulkan::Headers）
    )
else()
    #
    # 链接系统的 libvulkan.so
    #   使用 GPU 驱动提供的 loader，体积更小
    #
    find_library(VULKAN_LIBRARY
        NAMES vulkan-1 vulkan
        HINTS "$ENV{VULKAN_SDK}/Lib" "$ENV{VULKAN_SDK}/Lib32"
        REQUIRED)

    target_link_libraries(
        MyVulkanApp
        PRIVATE
        Vulkan::Headers      # externals 中的头文件
        ${VULKAN_LIBRARY}    # 系统 libvulkan.so
    )
endif()


