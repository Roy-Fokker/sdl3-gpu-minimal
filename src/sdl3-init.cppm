module;

// SDL 3 header
#include <SDL3/SDL.h>

export module sdl3_init;

import std;
import logs;

export namespace sdl3
{
	// Compilation mode
	constexpr auto IS_DEBUG = bool{ _DEBUG };

	// Deleter template, for use with SDL objects.
	// Allows use of SDL Objects with C++'s smart pointers, using SDL's destroy function
	template <auto fn>
	struct sdl_deleter
	{
		constexpr void operator()(auto *arg)
		{
			fn(arg);
		}
	};
	// Define SDL types with std::unique_ptr and custom deleter;
	using gpu_ptr    = std::unique_ptr<SDL_GPUDevice, sdl_deleter<SDL_DestroyGPUDevice>>;
	using window_ptr = std::unique_ptr<SDL_Window, sdl_deleter<SDL_DestroyWindow>>;

	// Structure containing all SDL objects that need to live for life of the program
	struct context
	{
		window_ptr window;
		gpu_ptr gpu;
	};

	// Initialize SDL with GPU
	auto init_context(uint32_t width, uint32_t height, std::string_view title) -> context
	{
		msg::info("Initialize SDL, GPU, and Window");

		auto result = SDL_Init(SDL_INIT_VIDEO);
		msg::error(result == true, "SDL could not initialize.");

		auto w      = static_cast<int>(width);
		auto h      = static_cast<int>(height);
		auto window = SDL_CreateWindow(title.data(), w, h, NULL);
		msg::error(window != nullptr, "Window could not be created.");

		auto gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV, IS_DEBUG, NULL);
		msg::error(gpu != nullptr, "Could not get GPU device.");

		auto gpu_driver_name = std::string_view{ SDL_GetGPUDeviceDriver(gpu) };
		msg::info(std::format("GPU Driver Name: {}", gpu_driver_name));

		result = SDL_ClaimWindowForGPUDevice(gpu, window);
		msg::error(result == true, "Could not claim window for gpu.");

		return {
			.window = window_ptr(window),
			.gpu    = gpu_ptr(gpu),
		};
	}

	// Destroy/Clean up SDL objects, especially cases not captured by custom deleter
	auto destroy_context(context &ctx)
	{
		msg::info("Destroy Window, GPU and SDL");

		SDL_ReleaseWindowFromGPUDevice(ctx.gpu.get(), ctx.window.get());

		ctx = {};
		SDL_Quit();
	}
}