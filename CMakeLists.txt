cmake_minimum_required(VERSION 3.20)
project(LearnMetal LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fetch Dependencies ---
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# metal-cpp (header-only, downloaded to third_party)
FetchContent_Declare(
    metal_cpp
    URL https://developer.apple.com/metal/cpp/files/metal-cpp_macOS15_iOS18.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(metal_cpp)

# --- Source Files ---
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.mm"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# --- Include Directories ---
# metal-cpp root must come BEFORE system frameworks so that
# <Foundation/Foundation.hpp> resolves to metal-cpp, not the system framework.
target_include_directories(${PROJECT_NAME} BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${metal_cpp_SOURCE_DIR}
)

# --- Compile Metal Shaders ---
# Compile .metal -> .air -> .metallib
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(METALLIB_OUTPUT ${CMAKE_BINARY_DIR}/default.metallib)

file(GLOB METAL_SOURCES "${SHADER_DIR}/*.metal")

set(AIR_FILES "")
foreach(METAL_FILE ${METAL_SOURCES})
    get_filename_component(METAL_NAME ${METAL_FILE} NAME_WE)
    set(AIR_FILE ${CMAKE_BINARY_DIR}/${METAL_NAME}.air)
    add_custom_command(
        OUTPUT ${AIR_FILE}
        COMMAND xcrun -sdk macosx metal -c ${METAL_FILE} -o ${AIR_FILE}
        DEPENDS ${METAL_FILE}
        COMMENT "Compiling Metal shader: ${METAL_NAME}.metal -> ${METAL_NAME}.air"
    )
    list(APPEND AIR_FILES ${AIR_FILE})
endforeach()

add_custom_command(
    OUTPUT ${METALLIB_OUTPUT}
    COMMAND xcrun -sdk macosx metallib ${AIR_FILES} -o ${METALLIB_OUTPUT}
    DEPENDS ${AIR_FILES}
    COMMENT "Linking Metal library: default.metallib"
)

add_custom_target(MetalShaders ALL DEPENDS ${METALLIB_OUTPUT})
add_dependencies(${PROJECT_NAME} MetalShaders)

# --- Link Libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    "-framework Metal"
    "-framework MetalKit"
    "-framework AppKit"
    "-framework Foundation"
    "-framework QuartzCore"
)

# --- Compile Options ---
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)
