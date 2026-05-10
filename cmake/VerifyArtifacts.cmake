set(required_files
    "${APP_PATH}"
    "${APP_DIR}/shaders_VSMain.cso"
    "${APP_DIR}/shaders_PSMain.cso"
    "${APP_DIR}/D3D12/D3D12Core.dll"
    "${APP_DIR}/D3D12/d3d12SDKLayers.dll"
    "${APP_DIR}/dxcompiler.dll"
    "${APP_DIR}/dxil.dll"
)

foreach(required_file IN LISTS required_files)
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Missing expected build artifact: ${required_file}")
    endif()
endforeach()

message(STATUS "D3D12HelloTriangle artifacts are present in ${APP_DIR}")
