cmake_minimum_required(VERSION 3.24)

project(MyVulkanProject
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)


#
# Vulkan-Headers — 始终从 GitHub 拉取，不依赖系统头文件
# OVERRIDE_FIND_PACKAGE 确保 Vulkan-Loader 的 find_package 也会找到这份头文件
#
FetchContent_Declare(
    Vulkan-Headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG        v1.4.313
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(Vulkan-Headers)


add_executable(
    MyVulkanApp
    src/main.cc
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    #
    # Debug 模式：从源码构建 Vulkan-Loader
    #   便于调试 loader 内部逻辑
    #
    FetchContent_Declare(
        Vulkan-Loader
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Loader.git
        GIT_TAG        v1.4.313
    )

    FetchContent_MakeAvailable(Vulkan-Loader)

    target_link_libraries(
        MyVulkanApp
        PRIVATE
        Vulkan::Headers     # FetchContent 拉取的头文件
        Vulkan::Loader      # 源码构建的 loader（内部已链接 Vulkan::Headers）
    )
else()
    #
    # Release 模式：链接系统的 libvulkan.so
    #   使用 GPU 驱动提供的 loader，体积更小
    #
    find_library(VULKAN_LIBRARY NAMES vulkan REQUIRED)

    target_link_libraries(
        MyVulkanApp
        PRIVATE
        Vulkan::Headers      # FetchContent 拉取的头文件
        ${VULKAN_LIBRARY}    # 系统 libvulkan.so
    )
endif()
