# D3D12HelloTriangle CMake Port

This directory is an isolated CMake build of `src/HelloTriangle`. The original
Visual Studio project under `src` is not modified.

The preset targets Windows ARM64:

```powershell
cmake --preset windows-arm64
cmake --build --preset windows-arm64-release
ctest --preset windows-arm64-release
```

The build discovers `dxc.exe` from the installed Windows SDK. The `d3dx12`
helper headers are vendored locally because they are header-only sample helpers,
not Windows SDK system headers.
