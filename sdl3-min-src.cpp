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

/*
 * Platform IO helpers
 */
namespace io
{
	// Simple function to read a file in binary mode.
	auto read_file(const std::filesystem::path &filename) -> std::vector<std::byte>
	{
		msg::info(std::format("Reading file: {}", filename.string()));

		auto file = std::ifstream(filename, std::ios::in | std::ios::binary);

		msg::error(file.good(), "failed to open file!");

		auto file_size = std::filesystem::file_size(filename);
		auto buffer    = std::vector<std::byte>(file_size);

		file.read(reinterpret_cast<char *>(buffer.data()), file_size);

		file.close();

		return buffer;
	}

	// Convience alias for a span of bytes
	using byte_span  = std::span<const std::byte>;
	using byte_spans = std::span<byte_span>;

	// Convert any object type to a span of bytes
	auto as_byte_span(const auto &src) -> byte_span
	{
		return std::span{
			reinterpret_cast<const std::byte *>(&src),
			sizeof(src)
		};
	}

	// Covert a any contiguous range type to a span of bytes
	auto as_byte_span(const std::ranges::contiguous_range auto &src) -> byte_span
	{
		auto src_span   = std::span{ src };      // convert to a span,
		auto byte_size  = src_span.size_bytes(); // so we can get size_bytes
		auto byte_start = reinterpret_cast<const std::byte *>(src.data());
		return { byte_start, byte_size };
	}
}

/*
 * SDL base objects initializations
 */
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

		auto gpu_driver_name = std::string_view{ SDL_GetGPUDeviceDriver(gpu) };
		msg::info(std::format("GPU Driver Name: {}", gpu_driver_name));

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

/*
 * SDL functions called every frame
 */
namespace frame
{
	// Deleter template, for use with SDL objects.
	// Allows use of SDL Objects with C++'s smart pointers, using SDL's destroy function
	// This version needs pointer to GPU.
	template <auto fn>
	struct sdl_gpu_deleter
	{
		SDL_GPUDevice *gpu = nullptr;

		constexpr void operator()(auto *arg)
		{
			fn(gpu, arg);
		}
	};
	// Typedefs for SDL objects that need GPU Device to properly destruct
	using sdl_free_gfx_pipeline = sdl_gpu_deleter<SDL_ReleaseGPUGraphicsPipeline>;
	using sdl_gfx_pipeline_ptr  = std::unique_ptr<SDL_GPUGraphicsPipeline, sdl_free_gfx_pipeline>;
	using sdl_free_gfx_shader   = sdl_gpu_deleter<SDL_ReleaseGPUShader>;
	using sdl_gpu_shader_ptr    = std::unique_ptr<SDL_GPUShader, sdl_free_gfx_shader>;
	using sdl_free_buffer       = sdl_gpu_deleter<SDL_ReleaseGPUBuffer>;
	using sdl_gpu_buffer_ptr    = std::unique_ptr<SDL_GPUBuffer, sdl_free_buffer>;

	// Structure to hold objects required to draw a frame
	struct frame_context
	{
		sdl_gfx_pipeline_ptr pipeline    = nullptr;
		sdl_gpu_buffer_ptr vertex_buffer = nullptr;
	};

	// Create GPU side shader using in-memory shader binary for specified stage
	auto load_gpu_shader(const base::sdl_context &ctx, const io::byte_span &bin, SDL_GPUShaderStage stage) -> sdl_gpu_shader_ptr
	{
		auto shader_format = [&]() -> SDL_GPUShaderFormat {
			auto backend_formats = SDL_GetGPUShaderFormats(ctx.gpu.get());

			if (backend_formats & SDL_GPU_SHADERFORMAT_DXIL)
				return SDL_GPU_SHADERFORMAT_DXIL;
			else
				return SDL_GPU_SHADERFORMAT_SPIRV;
		}();

		auto shader_info = SDL_GPUShaderCreateInfo{
			.code_size  = bin.size(),
			.code       = reinterpret_cast<const std::uint8_t *>(bin.data()),
			.entrypoint = "main",
			.format     = shader_format,
			.stage      = stage,
		};

		auto shader = SDL_CreateGPUShader(ctx.gpu.get(), &shader_info);
		msg::error(shader != nullptr, "Failed to create shader.");

		return sdl_gpu_shader_ptr(shader, sdl_free_gfx_shader{ ctx.gpu.get() });
	}

	// Create two pipelines with different fill modes, full and line.
	void create_pipelines(const base::sdl_context &ctx,
	                      uint32_t vertex_pitch,
	                      const std::span<const SDL_GPUVertexAttribute> vertex_attributes,
	                      frame::frame_context &rndr)
	{
		msg::info("Creating Pipelines.");

		auto vs_bin = io::read_file("shaders/vertex_buffer_triangle.vs_6_4.cso");
		auto fs_bin = io::read_file("shaders/raw_triangle.ps_6_4.cso");

		auto vs_shdr = load_gpu_shader(ctx, vs_bin, SDL_GPU_SHADERSTAGE_VERTEX);
		auto fs_shdr = load_gpu_shader(ctx, fs_bin, SDL_GPU_SHADERSTAGE_FRAGMENT);

		auto vertex_description = SDL_GPUVertexBufferDescription{
			.slot               = 0,
			.pitch              = vertex_pitch,
			.input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX,
			.instance_step_rate = 0,
		};

		auto vertex_input = SDL_GPUVertexInputState{
			.vertex_buffer_descriptions = &vertex_description,
			.num_vertex_buffers         = 1,
			.vertex_attributes          = vertex_attributes.data(),
			.num_vertex_attributes      = static_cast<uint32_t>(vertex_attributes.size()),
		};

		auto color_targets = std::array{
			SDL_GPUColorTargetDescription{
			  .format = SDL_GetGPUSwapchainTextureFormat(ctx.gpu.get(), ctx.window.get()),
			}
		};

		auto pipeline_info = SDL_GPUGraphicsPipelineCreateInfo{
			.vertex_shader      = vs_shdr.get(),
			.fragment_shader    = fs_shdr.get(),
			.vertex_input_state = vertex_input,
			.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.rasterizer_state   = {
				.fill_mode = SDL_GPU_FILLMODE_FILL,
            },
			.target_info = {
			  .color_target_descriptions = color_targets.data(),
			  .num_color_targets         = static_cast<uint32_t>(color_targets.size()),
			},
		};

		auto pipeline = SDL_CreateGPUGraphicsPipeline(ctx.gpu.get(), &pipeline_info);
		msg::error(pipeline != nullptr, "Failed to create fill pipeline.");

		rndr.pipeline = sdl_gfx_pipeline_ptr(pipeline, sdl_free_gfx_pipeline{ ctx.gpu.get() });
	}

	// Create Vertex Buffer, and using a Transfer Buffer upload vertex data
	void create_and_copy_vertices(const base::sdl_context &ctx,
	                              const io::byte_span vertices,
	                              frame_context &rndr)
	{
		msg::info("Create Vertex Buffer.");

		auto buffer_size = static_cast<uint32_t>(vertices.size());

		auto buffer_info = SDL_GPUBufferCreateInfo{
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size  = buffer_size,
		};

		auto vertex_buffer = SDL_CreateGPUBuffer(ctx.gpu.get(), &buffer_info);
		rndr.vertex_buffer = sdl_gpu_buffer_ptr(vertex_buffer, sdl_free_buffer{ ctx.gpu.get() });
		msg::error(vertex_buffer != nullptr, "Could not create GPU Vertex Buffer.");

		msg::info("Create Transfer Buffer.");
		auto transfer_buffer_info = SDL_GPUTransferBufferCreateInfo{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size  = buffer_size,
		};

		auto transfer_buffer = SDL_CreateGPUTransferBuffer(ctx.gpu.get(), &transfer_buffer_info);
		msg::error(transfer_buffer != nullptr, "Could not create GPU Transfer Buffer");

		msg::info("Copy vertices to Transfer Buffer.");
		auto *data = SDL_MapGPUTransferBuffer(ctx.gpu.get(), transfer_buffer, false);

		std::memcpy(data, vertices.data(), vertices.size());

		SDL_UnmapGPUTransferBuffer(ctx.gpu.get(), transfer_buffer);

		msg::info("Upload from Transfer Buffer to Vertex Buffer.");
		auto upload_cmd = SDL_AcquireGPUCommandBuffer(ctx.gpu.get());
		auto copypass   = SDL_BeginGPUCopyPass(upload_cmd);

		auto src = SDL_GPUTransferBufferLocation{
			.transfer_buffer = transfer_buffer,
			.offset          = 0,
		};
		auto dst = SDL_GPUBufferRegion{
			.buffer = vertex_buffer,
			.offset = 0,
			.size   = buffer_size
		};

		SDL_UploadToGPUBuffer(copypass, &src, &dst, false);

		SDL_EndGPUCopyPass(copypass);
		SDL_SubmitGPUCommandBuffer(upload_cmd);
		SDL_ReleaseGPUTransferBuffer(ctx.gpu.get(), transfer_buffer);
	}

	// Initialize all Frame objects
	auto init(const base::sdl_context &ctx,
	          const io::byte_span vertices,
	          uint32_t vertex_count,
	          const std::span<const SDL_GPUVertexAttribute> vertex_attributes) -> frame_context
	{
		msg::info("Initialize frame objects");

		auto rndr = frame_context{};
		create_pipelines(ctx, static_cast<uint32_t>(vertices.size() / vertex_count), vertex_attributes, rndr);
		create_and_copy_vertices(ctx, vertices, rndr);
		// create_viewport_and_scissor(rndr);

		return rndr;
	}

	// clean up frame objects
	void destroy([[maybe_unused]] const base::sdl_context &ctx, frame_context &rndr)
	{
		msg::info("Destroy frame objects");

		// must do this before calling SDL_Quit or freeing GPU device
		rndr = {};
	}

	// Get Swapchain Image/Texture, wait if none is available
	// Does not use smart pointer as lifetime of swapchain texture is managed by SDL
	auto get_swapchain_texture(const base::sdl_context &ctx, SDL_GPUCommandBuffer *cmd_buf) -> SDL_GPUTexture *
	{
		auto sc_tex = (SDL_GPUTexture *)nullptr;

		auto res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd_buf, ctx.window.get(), &sc_tex, NULL, NULL);
		msg::error(res == true, "Wait and acquire GPU swapchain texture failed.");
		msg::error(sc_tex != nullptr, "Swapchain texture is null. Is window minimized?");

		return sc_tex;
	}

	// Draw to window using Command Buffer and Renderpass
	void draw(const base::sdl_context &ctx, const frame_context &rndr)
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

		SDL_BindGPUGraphicsPipeline(renderpass, rndr.pipeline.get());

		auto vertex_bindings = SDL_GPUBufferBinding{
			.buffer = rndr.vertex_buffer.get(),
			.offset = 0
		};
		SDL_BindGPUVertexBuffers(renderpass, 0, &vertex_bindings, 1);

		SDL_DrawGPUPrimitives(renderpass, 3, 1, 0, 0);

		SDL_EndGPURenderPass(renderpass);

		SDL_SubmitGPUCommandBuffer(cmd_buf);
	}
}

/*
 * To manage application specific state data
 */
namespace app
{
	struct pos_clr_vertex
	{
		float x, y, z;
		uint8_t r, g, b, a;

		constexpr static auto vertex_attributes = std::array{
			SDL_GPUVertexAttribute{
			  .location    = 0,
			  .buffer_slot = 0,
			  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
			  .offset      = 0,
			},
			SDL_GPUVertexAttribute{
			  .location    = 1,
			  .buffer_slot = 0,
			  .format      = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
			  .offset      = sizeof(float) * 3,
			}
		};
	};
}

auto main() -> int
{
	constexpr auto app_title     = "SDL3 GPU minimal example."sv;
	constexpr auto window_width  = 1920;
	constexpr auto window_height = 1080;

	auto ctx = base::init(window_width, window_height, app_title);

	auto triangle = std::array{
		app::pos_clr_vertex{ -1.f, -1.f, 0.f, 255, 0, 0, 255 },
		app::pos_clr_vertex{ 1.f, -1.f, 0.f, 0, 255, 0, 255 },
		app::pos_clr_vertex{ 0.f, 1.f, 0.f, 0, 0, 255, 255 },
	};

	auto rndr = frame::init(ctx,
	                        io::as_byte_span(triangle),
	                        static_cast<uint32_t>(triangle.size()),
	                        app::pos_clr_vertex::vertex_attributes);

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
			else if (evnt.type == SDL_EVENT_KEY_DOWN)
			{
				switch (evnt.key.key)
				{
				case SDLK_ESCAPE:
					quit = true;
					break;
				default:
					break;
				}
			}
		}

		// update(rndr, stt);

		frame::draw(ctx, rndr);
	}

	frame::destroy(ctx, rndr);

	base::destroy(ctx);

	return 0;
}