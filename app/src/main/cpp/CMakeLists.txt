# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project("myapplication")

# ---------------------------------------------------------------------------
# Shader compilation: GLSL -> SPIR-V (via the NDK's glslc), then embed the
# resulting SPIR-V bytecode into a generated C header (ShaderSPIRV.h) so the
# app ships pre-compiled shaders instead of GLSL source strings.
# ---------------------------------------------------------------------------
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SPIRV_DIR ${CMAKE_CURRENT_BINARY_DIR}/spv)
file(MAKE_DIRECTORY ${SPIRV_DIR})

# The NDK ships glslc under shader-tools/<host-tag>/.
find_program(GLSLC_EXECUTABLE NAMES glslc
        HINTS
            "${ANDROID_NDK}/shader-tools/darwin-x86_64"
            "${ANDROID_NDK}/shader-tools/darwin-aarch64"
            "${ANDROID_NDK}/shader-tools/linux-x86_64"
            "${ANDROID_NDK}/shader-tools/windows-x86_64"
            "${ANDROID_NDK}/shader-tools/windows-x86_64.exe"
        NO_DEFAULT_PATH)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found under the NDK shader-tools directory.")
endif()

set(VERT_SPV ${SPIRV_DIR}/shader.vert.spv)
set(FRAG_SPV ${SPIRV_DIR}/shader.frag.spv)
add_custom_command(
        OUTPUT ${VERT_SPV} ${FRAG_SPV}
        COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.3 -o ${VERT_SPV} ${SHADER_DIR}/shader.vert
        COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.3 -o ${FRAG_SPV} ${SHADER_DIR}/shader.frag
        DEPENDS ${SHADER_DIR}/shader.vert ${SHADER_DIR}/shader.frag
        COMMENT "Compiling GLSL shaders to SPIR-V")

# Generate a header embedding the SPIR-V bytes (kVertSpv / kFragSpv).
set(SPIRV_HEADER ${CMAKE_CURRENT_BINARY_DIR}/ShaderSPIRV.h)
add_custom_command(
        OUTPUT ${SPIRV_HEADER}
        COMMAND ${CMAKE_COMMAND}
                -Dspv_dir=${SPIRV_DIR}
                -Dout=${SPIRV_HEADER}
                -P ${SHADER_DIR}/gen_spirv_header.cmake
        DEPENDS ${VERT_SPV} ${FRAG_SPV} ${SHADER_DIR}/gen_spirv_header.cmake
        COMMENT "Embedding SPIR-V into ShaderSPIRV.h")
add_custom_target(generate_shaders DEPENDS ${SPIRV_HEADER})

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
add_library(myapplication SHARED
        main.cpp
        AndroidOut.cpp
        Renderer.cpp
        Shader.cpp
        TextureAsset.cpp
        Utility.cpp
        VulkanUtil.cpp)

# Make the generated ShaderSPIRV.h discoverable.
target_include_directories(myapplication PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Ensure the embedded SPIR-V header exists before the sources compile.
add_dependencies(myapplication generate_shaders)

# Searches for a package provided by the game activity dependency
find_package(game-activity REQUIRED CONFIG)

# Forces the linker to keep the JNI entry point for GameActivity
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

# Configure libraries CMake uses to link your target library.
target_link_libraries(myapplication
        # The game activity
        game-activity::game-activity_static

        # Vulkan replaces EGL/GLESv3 for rendering
        vulkan

        # Android platform: AAssetManager, ANativeWindow
        android
        # AImageDecoder (PNG decoding) lives in libjnigraphics.so
        jnigraphics
        log)