# Minimal SDL3 GPU example
---

Learning how to use new SDL3-GPU features with C++.

## Project Setup
Uses `CMake` with `presets` and `C++23` with `std modules`. `Ninja` is build tool.
All the code is in `sdl3-min-src.cpp`.
External dependencies are managed via `vcpkg`. `SDL3` is consumed as an dependency via vcpkg.
`.clang-tidy` and `.clang-format` to maintain code formatting rules.
`.clangd` to experiment with using clangd as lsp, it doesn't like modules.

Project uses `CMakePresets` json file to control platform some specific compiler configuration.
As well as build (`ninja`) and external dependency (`vcpkg`) configuration.

Windows only for the moment, but it shouldn't have issues compiling on linux with **some** tweaks. e.g. adding Linux build presets.

### Shader Compilation
CMake script uses a custom function to take list of shader source files and output compiled bytecode to `bin/shaders` directory. It also makes shader bytecode output a dependency of the application. 
So compilation error in shader will stop the build process.

As mentioned above, by default, code expects `Windows`. As it uses DirectX's DXC compiler to convert HLSL to DXIL bytecode. Application prefers DX12 over Vulkan. I suppose if the order were changed, it would need SPIR-V bytecode. Which would require changing shader complier from DirectX's DXC to Vulkan's DXC which is shipped with Vulkan SDK. Windows SDK's DXC cannot compile to SPIR-V, and Vulkan SDK's DXC cannot compile to DXIL. At least I wasn't able to make this work with versions I have installed on system.

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

# to clean the project outputs
cmake --build --preset windows-debug --target clean
```

## Learning Progress
Each tag, modifies previous tag's sources.
- [Clear Screen](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/0-clear-screen): Clear window with specified color.
- [Basic Triangle](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/1-raw-triangle): Draw a simple colored triangle. Press 1, 2, or 3, to change pipeline type, viewport, and scissor respectively
- [Vertex Buffer Triangle](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/2-vertex-buffer): Draw a triangle using vertex buffer.
- [Cull Modes](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/3-cull-modes): Draw triangle with Backface/Frontface Culling, Clockwise/Counter-Clockwise vertex ordering. Press 1-6 to toggle cull-mode + vertex-order combinations.
- [Basic Stencil](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/4-basic-stencil): Basic stencil operation. Substract mask from triangle.
- [Index Buffer Shape](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/5-index-buffer): Draw a shape using Index Buffer.
- [Instance Shapes](https://github.com/Roy-Fokker/sdl3-gpu-minimal/tree/6-instance-shapes): Slight tweak of previous, simple change to vertex shader and increase instance count in cpp.
- Basic Texture
- Basic Depth Buffer

## References
- <https://github.com/TheSpydog/SDL_gpu_examples> : my code is basically following this repo as example/source. But without "framework" portion so I can understand it better.