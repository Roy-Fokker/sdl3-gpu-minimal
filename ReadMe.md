# Minimal SDL3 GPU example
---

Learning how to use new SDL3-GPU features with C++.

## Project Setup
Uses `CMake` with `presets` and `C++23` with `std modules`. `Ninja` is build tool.
All the code is in `sdl3-min-src.cpp`.
External dependencies are managed via `vcpkg`. `SDL3` is consumed as an dependency via vcpkg.
`.clang-tidy` and `.clang-format` to maintain code formatting rules.
`.clangd` to experiment with using clangd as lsp, it doesn't like modules.

Windows only for the moment, but it shouldn't have issues compiling on linux with some tweaks.
For example, changing shader complier from DirectX's DXC to Vulkan's DXC which is shipped with Vulkan SDK.
Windows SDK's DXC cannot compile to SPIR-V, and Vulkan SDK's DXC cannot compile to DXIL. At least I wasn't able to make this work with version I have installed on system. Both are seemingly maintained by Microsoft.

## Prerequisites
Build Tools
- CMake 3.31+
- vcpkg
- ninja 1.12+
- Windows SDK & MSVC build tools
- clang-tidy & clang-format

## Configure and Build
In project root, from VS command prompt
```shell
# to configure the project
cmake --preset windows-default

# to build the project
cmake --build --preset windows-debug
```

## Learning Progress
- [Clear Screen](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/0-clear-screen): Clear window with specified color.
- [Basic Triangle](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/1-raw-triangle): Draw a simple colored triangle. Press 1, 2, or 3, to change pipeline type, viewport, and scissor respectively

## References
- <https://github.com/TheSpydog/SDL_gpu_examples> : my code is basically following this repo as example/source. But without "framework" portion so I can understand it better.