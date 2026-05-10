cmake_minimum_required(VERSION 3.24)

project(D3D12HelloTriangleCMake LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "D3D12HelloTriangle is a Windows-only sample.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "D3D12HelloTriangle requires the Microsoft Visual C++ toolchain.")
endif()

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    if(NOT CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
        message(FATAL_ERROR "Configure for ARM64, for example: cmake --preset windows-arm64")
    endif()
endif()

string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" host_processor)
if(host_processor MATCHES "arm64|aarch64")
    set(DXC_TOOL_PLATFORM arm64)
else()
    set(DXC_TOOL_PLATFORM x64)
endif()

function(find_windows_sdk_tool out_var tool_name)
    set(search_dirs)

    if(DEFINED ENV{WindowsSdkDir})
        file(TO_CMAKE_PATH "$ENV{WindowsSdkDir}" sdk_root_from_env)
        if(DEFINED ENV{WindowsSDKVersion})
            list(APPEND search_dirs "${sdk_root_from_env}/bin/$ENV{WindowsSDKVersion}/${DXC_TOOL_PLATFORM}")
        endif()

        file(GLOB env_sdk_tool_dirs LIST_DIRECTORIES true
            "${sdk_root_from_env}/bin/*/${DXC_TOOL_PLATFORM}")
        list(APPEND search_dirs ${env_sdk_tool_dirs})
    endif()

    cmake_host_system_information(
        RESULT kits_root
        QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/Microsoft/Windows Kits/Installed Roots"
        VALUE "KitsRoot10"
        VIEW BOTH
        ERROR_VARIABLE kits_root_error)

    if(kits_root)
        file(TO_CMAKE_PATH "${kits_root}" kits_root)
        file(GLOB registry_sdk_tool_dirs LIST_DIRECTORIES true
            "${kits_root}/bin/*/${DXC_TOOL_PLATFORM}")
        list(APPEND search_dirs ${registry_sdk_tool_dirs})
    endif()

    if(search_dirs)
        list(REMOVE_DUPLICATES search_dirs)
        list(SORT search_dirs COMPARE NATURAL ORDER DESCENDING)
    endif()

    find_program(found_tool NAMES "${tool_name}" HINTS ${search_dirs})
    if(NOT found_tool)
        message(FATAL_ERROR "Could not find ${tool_name}. Install the Windows SDK or run CMake from a Visual Studio developer environment.")
    endif()

    set(${out_var} "${found_tool}" PARENT_SCOPE)
endfunction()

find_windows_sdk_tool(DXC_EXE dxc.exe)
message(STATUS "Using DXC: ${DXC_EXE}")

add_executable(D3D12HelloTriangle WIN32
    HelloTriangle/D3D12HelloTriangle.cpp
    HelloTriangle/D3D12HelloTriangle.h
    HelloTriangle/DXSample.cpp
    HelloTriangle/DXSample.h
    HelloTriangle/DXSampleHelper.h
    HelloTriangle/Main.cpp
    HelloTriangle/shaders.hlsl
    HelloTriangle/stdafx.cpp
    HelloTriangle/stdafx.h
    HelloTriangle/Win32Application.cpp
    HelloTriangle/Win32Application.h
)

target_compile_features(D3D12HelloTriangle PRIVATE cxx_std_17)

target_compile_definitions(D3D12HelloTriangle PRIVATE
    WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

target_compile_options(D3D12HelloTriangle PRIVATE
    /W3
    /sdl
    $<$<CONFIG:Release>:/O2>
)

target_include_directories(D3D12HelloTriangle PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/HelloTriangle"
)

target_link_libraries(D3D12HelloTriangle PRIVATE
    d3d12
    dxgi
    d3dcompiler
    delayimp
)

target_link_options(D3D12HelloTriangle PRIVATE /DELAYLOAD:d3d12.dll)

set_property(TARGET D3D12HelloTriangle PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:D3D12HelloTriangle>")
set_source_files_properties(HelloTriangle/shaders.hlsl PROPERTIES VS_TOOL_OVERRIDE "None")

add_custom_command(TARGET D3D12HelloTriangle POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:D3D12HelloTriangle>/D3D12"
    COMMAND "${DXC_EXE}" -nologo -T vs_6_0 -E VSMain -Zi -Qembed_debug
            -Fo "$<TARGET_FILE_DIR:D3D12HelloTriangle>/shaders_VSMain.cso"
            "${CMAKE_CURRENT_LIST_DIR}/HelloTriangle/shaders.hlsl"
    COMMAND "${DXC_EXE}" -nologo -T ps_6_0 -E PSMain -Zi -Qembed_debug
            -Fo "$<TARGET_FILE_DIR:D3D12HelloTriangle>/shaders_PSMain.cso"
            "${CMAKE_CURRENT_LIST_DIR}/HelloTriangle/shaders.hlsl"
    VERBATIM
)

enable_testing()

add_test(NAME D3D12HelloTriangleArtifacts
    COMMAND "${CMAKE_COMMAND}"
            -DAPP_PATH=$<TARGET_FILE:D3D12HelloTriangle>
            -DAPP_DIR=$<TARGET_FILE_DIR:D3D12HelloTriangle>
            -P "${CMAKE_CURRENT_LIST_DIR}/cmake/VerifyArtifacts.cmake"
)
