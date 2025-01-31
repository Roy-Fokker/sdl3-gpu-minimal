# Minimal SDL3 GPU example
---

Learning how to use new SDL3-gpu features with C++.

## Project Setup
Uses `CMake` with `presets` and `C++23` with `std modules`. `Ninja` is build tool.
All the code is in `sdl3-min-src.cpp`.
External dependencies are managed via `vcpkg`. `SDL3` is consumed as an external dependency.
`.clang-tidy` and `.clang-format` to maintain code formatting rules.
`.clangd` to experiment with using clangd as lsp, it doesn't like modules.

Windows only for the moment, but it shouldn't have issues compiling on linux with some tweaks.

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