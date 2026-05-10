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

set(D3D12_PACKAGE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../src/packages/Microsoft.Direct3D.D3D12.1.618.3/build/native"
    CACHE PATH "Microsoft.Direct3D.D3D12 native package root")
set(DXC_PACKAGE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../src/packages/Microsoft.Direct3D.DXC.1.8.2505.32/build/native"
    CACHE PATH "Microsoft.Direct3D.DXC native package root")

foreach(required_path IN ITEMS
        "${D3D12_PACKAGE_ROOT}/include/d3d12.h"
        "${D3D12_PACKAGE_ROOT}/include/d3dx12/d3dx12.h"
        "${D3D12_PACKAGE_ROOT}/bin/arm64/D3D12Core.dll"
        "${DXC_PACKAGE_ROOT}/include/dxcapi.h"
        "${DXC_PACKAGE_ROOT}/bin/arm64/dxc.exe"
        "${DXC_PACKAGE_ROOT}/lib/arm64/dxcompiler.lib")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Required package file is missing: ${required_path}")
    endif()
endforeach()

string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" host_processor)
if(host_processor MATCHES "arm64|aarch64")
    set(DXC_TOOL_PLATFORM arm64)
else()
    set(DXC_TOOL_PLATFORM x64)
endif()

set(D3D_PLATFORM arm64)
set(DXC_EXE "${DXC_PACKAGE_ROOT}/bin/${DXC_TOOL_PLATFORM}/dxc.exe")

if(NOT EXISTS "${DXC_EXE}")
    message(FATAL_ERROR "Host DXC executable is missing: ${DXC_EXE}")
endif()

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
    "${D3D12_PACKAGE_ROOT}"
    "${D3D12_PACKAGE_ROOT}/include"
    "${D3D12_PACKAGE_ROOT}/include/d3dx12"
    "${DXC_PACKAGE_ROOT}/include"
)

target_link_libraries(D3D12HelloTriangle PRIVATE
    d3d12
    dxgi
    d3dcompiler
    delayimp
    "${DXC_PACKAGE_ROOT}/lib/${D3D_PLATFORM}/dxcompiler.lib"
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

function(copy_runtime_file source_file destination_subdir)
    if(EXISTS "${source_file}")
        add_custom_command(TARGET D3D12HelloTriangle POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${source_file}"
                    "$<TARGET_FILE_DIR:D3D12HelloTriangle>/${destination_subdir}"
            VERBATIM
        )
    endif()
endfunction()

copy_runtime_file("${D3D12_PACKAGE_ROOT}/bin/${D3D_PLATFORM}/D3D12Core.dll" "D3D12")
copy_runtime_file("${D3D12_PACKAGE_ROOT}/bin/${D3D_PLATFORM}/D3D12Core.pdb" "D3D12")
copy_runtime_file("${D3D12_PACKAGE_ROOT}/bin/${D3D_PLATFORM}/d3d12SDKLayers.dll" "D3D12")
copy_runtime_file("${D3D12_PACKAGE_ROOT}/bin/${D3D_PLATFORM}/d3d12SDKLayers.pdb" "D3D12")
copy_runtime_file("${DXC_PACKAGE_ROOT}/bin/${D3D_PLATFORM}/dxcompiler.dll" ".")
copy_runtime_file("${DXC_PACKAGE_ROOT}/bin/${D3D_PLATFORM}/dxcompiler.pdb" ".")
copy_runtime_file("${DXC_PACKAGE_ROOT}/bin/${D3D_PLATFORM}/dxil.dll" ".")
copy_runtime_file("${DXC_PACKAGE_ROOT}/bin/${D3D_PLATFORM}/dxil.pdb" ".")

enable_testing()

add_test(NAME D3D12HelloTriangleArtifacts
    COMMAND "${CMAKE_COMMAND}"
            -DAPP_PATH=$<TARGET_FILE:D3D12HelloTriangle>
            -DAPP_DIR=$<TARGET_FILE_DIR:D3D12HelloTriangle>
            -P "${CMAKE_CURRENT_LIST_DIR}/cmake/VerifyArtifacts.cmake"
)
