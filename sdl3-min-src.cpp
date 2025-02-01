#include <cassert>

// SDL 3 header
#include <SDL3/SDL.h>

// Standard Library module
import std;

// literal suffixes for strings, string_view, etc
using namespace std::literals;

/*
 * Get colored logging messages
 */
namespace msg
{
	namespace color
	{
		// Regular Colors
		constexpr auto BLK = "\033[0;30m";
		constexpr auto RED = "\033[0;31m";
		constexpr auto GRN = "\033[0;32m";
		constexpr auto YEL = "\033[0;33m";
		constexpr auto BLU = "\033[0;34m";
		constexpr auto MAG = "\033[0;35m";
		constexpr auto CYN = "\033[0;36m";
		constexpr auto WHT = "\033[0;37m";

		// Bright/Bold Colors
		constexpr auto BBLK = "\033[1;30m";
		constexpr auto BRED = "\033[1;31m";
		constexpr auto BGRN = "\033[1;32m";
		constexpr auto BYEL = "\033[1;33m";
		constexpr auto BBLU = "\033[1;34m";
		constexpr auto BMAG = "\033[1;35m";
		constexpr auto BCYN = "\033[1;36m";
		constexpr auto BWHT = "\033[1;37m";

		// Reset Color and Style
		constexpr auto RESET = "\033[0m";
	}

	// if there is an error, print message then assert
	void error(bool condition,
	           const std::string_view message,
	           const std::source_location location = std::source_location::current())
	{
		if (condition == true)
			return;

		std::println("{}[Error]: {}, in {} @ {}{}",
		             color::BRED,
		             message,
		             location.function_name(),
		             location.line(),
		             color::RESET);
		assert(condition);
	}

	// print information messages
	void info(const std::string_view message)
	{
		std::println("{}[Info]: {}{}",
		             color::GRN,
		             message,
		             color::RESET);
	}
}
namespace base
{
	// Compilation mode
	constexpr auto debug =
#ifdef _DEBUG
		true;
#else
		false;
#endif

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
	using sdl_gpu_ptr    = std::unique_ptr<SDL_GPUDevice, sdl_deleter<SDL_DestroyGPUDevice>>;
	using sdl_window_ptr = std::unique_ptr<SDL_Window, sdl_deleter<SDL_DestroyWindow>>;

	// Structure containing all SDL objects that need to live for life of the program
	struct sdl_context
	{
		sdl_gpu_ptr gpu;
		sdl_window_ptr window;
	};

	// Initialize SDL with GPU
	auto init(int width, int height, std::string_view title) -> sdl_context
	{
		msg::info("Initialize SDL, GPU, and Window");

		auto res = SDL_Init(SDL_INIT_VIDEO);
		msg::error(res == true, "SDL could not initialize.");

		// get available GPU
		auto gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV, debug, NULL);
		msg::error(gpu != nullptr, "Could not get GPU device.");

		// make a window
		auto window = SDL_CreateWindow(title.data(), width, height, NULL);
		msg::error(window != nullptr, "Window could not be created.");

		// get GPU surface for window
		res = SDL_ClaimWindowForGPUDevice(gpu, window);
		msg::error(res == true, "Could not claim window for gpu.");

		// return the sdl_context, with objects wrapped in unique_ptr
		return {
			.gpu    = sdl_gpu_ptr(gpu),
			.window = sdl_window_ptr(window),
		};
	}

	// Destroy/Clean up SDL objects, especially cases not captured by custom deleter
	void destroy(sdl_context &ctx)
	{
		msg::info("Destroy Window, GPU and SDL");

		SDL_ReleaseWindowFromGPUDevice(ctx.gpu.get(), ctx.window.get());

		ctx = {};
		SDL_Quit();
	}
}

namespace frame
{
	// Get Swapchain Image/Texture, wait if none is available
	auto get_swapchain_texture(base::sdl_context &ctx, SDL_GPUCommandBuffer *cmd_buf) -> SDL_GPUTexture *
	{
		auto sc_tex = (SDL_GPUTexture *)nullptr;

		auto res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd_buf, ctx.window.get(), &sc_tex, NULL, NULL);
		msg::error(res == true, "Wait and acquire GPU swapchain texture failed.");
		msg::error(sc_tex != nullptr, "Swapchain texture is null. Is window minimized?");

		return sc_tex;
	}

	// Draw to window using Command Buffer and Renderpass
	void draw(base::sdl_context &ctx)
	{
		auto cmd_buf = SDL_AcquireGPUCommandBuffer(ctx.gpu.get());
		msg::error(cmd_buf != nullptr, "Failed to acquire command buffer.");

		auto sc_image          = get_swapchain_texture(ctx, cmd_buf);
		auto color_target_info = SDL_GPUColorTargetInfo{
			.texture     = sc_image,
			.clear_color = SDL_FColor{ 0.4f, 0.4f, 0.5f, 1.0f },
			.load_op     = SDL_GPU_LOADOP_CLEAR,
			.store_op    = SDL_GPU_STOREOP_STORE,
		};

		auto renderpass = SDL_BeginGPURenderPass(cmd_buf, &color_target_info, 1, NULL);

		SDL_EndGPURenderPass(renderpass);

		SDL_SubmitGPUCommandBuffer(cmd_buf);
	}
}

auto main() -> int
{
	constexpr auto app_title     = "SDL3 GPU minimal example."sv;
	constexpr auto window_width  = 1920;
	constexpr auto window_height = 1080;

	auto ctx = base::init(window_width, window_height, app_title);

	auto quit = false;
	auto evnt = SDL_Event{};
	while (not quit)
	{
		while (SDL_PollEvent(&evnt))
		{
			if (evnt.type == SDL_EVENT_QUIT or (evnt.type == SDL_EVENT_KEY_DOWN and evnt.key.key == SDLK_ESCAPE))
			{
				quit = true;
			}
		}

		frame::draw(ctx);
	}

	base::destroy(ctx);

	return 0;
}