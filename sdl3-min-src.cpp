#include <cassert>
#include <SDL3/SDL.h>

import std;

using namespace std::literals;

namespace base
{
	constexpr auto debug =
#ifdef _DEBUG
		true;
#else
		false;
#endif

	template <auto fn>
	struct sdl_deleter
	{
		constexpr void operator()(auto *arg)
		{
			fn(arg);
		}
	};
	using sdl_gpu_ptr    = std::unique_ptr<SDL_GPUDevice, sdl_deleter<SDL_DestroyGPUDevice>>;
	using sdl_window_ptr = std::unique_ptr<SDL_Window, sdl_deleter<SDL_DestroyWindow>>;

	struct sdl_context
	{
		sdl_gpu_ptr gpu;
		sdl_window_ptr window;
	};

	auto init(int width, int height, std::string_view title) -> sdl_context
	{
		auto res = SDL_Init(SDL_INIT_VIDEO);
		assert(res == true && "SDL could not initialize.");

		auto ctx = sdl_context{};

		auto gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV, debug, NULL);
		assert(gpu != nullptr && "Could not get GPU device.");

		auto window = SDL_CreateWindow(title.data(), width, height, NULL);
		assert(window != nullptr && "Window could not be created.");

		res = SDL_ClaimWindowForGPUDevice(gpu, window);
		assert(res == true && "Could not claim window for gpu.");

		return {
			.gpu    = sdl_gpu_ptr(gpu),
			.window = sdl_window_ptr(window),
		};
	}

	void destroy([[maybe_unused]] sdl_context &ctx)
	{
		SDL_ReleaseWindowFromGPUDevice(ctx.gpu.get(), ctx.window.get());

		ctx = {};
		SDL_Quit();
	}
}

namespace frame
{
	auto get_swapchain_texture(base::sdl_context &ctx, SDL_GPUCommandBuffer *cmd_buf) -> SDL_GPUTexture *
	{
		auto sc_tex = (SDL_GPUTexture *)nullptr;

		auto res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd_buf, ctx.window.get(), &sc_tex, NULL, NULL);
		assert(res == true && "Wait and acquire GPU swapchain texture failed.");
		assert(sc_tex != nullptr && "Swapchain texture is null.");

		return sc_tex;
	}

	void draw(base::sdl_context &ctx)
	{
		auto cmd_buf = SDL_AcquireGPUCommandBuffer(ctx.gpu.get());
		assert(cmd_buf != nullptr && "Failed to acquire command buffer.");

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
			if (evnt.type == SDL_EVENT_QUIT)
			{
				quit = true;
			}
		}

		frame::draw(ctx);
	}

	base::destroy(ctx);

	return 0;
}