// For assert macro
#include <cassert>

// SDL 3 header
#include <SDL3/SDL.h>

// DDS and KTX texture file loader
// DDSKTX_IMPLEMENT must be defined in exactly one translation unit
// doesn't matter for this example, but it's good practice to define it in the same file as the implementation
#define DDSKTX_IMPLEMENT
#include "dds-ktx.h"

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
	void error(bool condition, // true condition, if this is false it is an error
	           const std::string_view message,
	           const std::source_location location = std::source_location::current())
	{
		if (condition == true)
			return;

		auto sdl_err = SDL_GetError();

		std::println("{}[Error]: {}, in {} @ {}{}\n"
		             "\t[SDL Error]: {}",
		             color::BRED,
		             message,
		             location.function_name(),
		             location.line(),
		             color::RESET,
		             sdl_err);
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

	// void pointer offset
	auto offset_ptr(void *ptr, std::ptrdiff_t offset) -> void *
	{
		return reinterpret_cast<std::byte *>(ptr) + offset;
	}

	struct image_data
	{
		struct image_header
		{
			SDL_GPUTextureFormat format;
			uint32_t width;
			uint32_t height;
			uint32_t depth;
			uint32_t layer_count;
			uint32_t mipmap_count;
		};

		struct sub_image
		{
			uint32_t layer_index;
			uint32_t mipmap_index;
			uintptr_t offset;
			uint32_t width;
			uint32_t height;
		};

		image_header header;
		std::vector<sub_image> sub_images;
		std::vector<std::byte> data;
	};

	// Read DDS/KTX image file and return image_data object with file contents
	auto read_image_file(const std::filesystem::path &filename) -> image_data
	{
		// Lambda to convert from DDS/KTX format to SDL format.
		// Not all cases are covered.
		auto to_sdl_format = [](ddsktx_format format) -> SDL_GPUTextureFormat {
			using sf = SDL_GPUTextureFormat;

			switch (format)
			{
			case DDSKTX_FORMAT_BC1: // DXT1
				return sf::SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
			case DDSKTX_FORMAT_BC2: // DXT3
				return sf::SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM;
			case DDSKTX_FORMAT_BC3: // DXT5
				return sf::SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
			case DDSKTX_FORMAT_BC4: // ATI1
				return sf::SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM;
			case DDSKTX_FORMAT_BC5: // ATI2
				return sf::SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM;
			case DDSKTX_FORMAT_BC6H: // BC6H
				return sf::SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT;
			case DDSKTX_FORMAT_BC7: // BC7
				return sf::SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
			default:
				break;
			}

			msg::error(false, "Unmapped ddsktx_format, no conversion available");
			return sf::SDL_GPU_TEXTUREFORMAT_INVALID;
		};

		// Load the file into memory
		auto image_file_data = read_file(filename);
		// get start pointer and size
		auto image_data_ptr  = static_cast<void *>(image_file_data.data());
		auto image_data_size = static_cast<int32_t>(image_file_data.size());

		auto image_data_header = ddsktx_texture_info{};
		auto image_parse_error = ddsktx_error{};

		// Parse DDS/KTX image file.
		auto result = ddsktx_parse(&image_data_header,
		                           image_data_ptr,
		                           image_data_size,
		                           &image_parse_error);
		msg::error(result == true, "Failed to parse image file data.");

		// alias views and ranges namespaces to save on visual noise
		namespace vw = std::views;
		namespace rg = std::ranges;

		// make ranges that count from 0 upto but not including #
		auto layer_rng  = vw::iota(0, image_data_header.num_layers);
		auto mipmap_rng = vw::iota(0, image_data_header.num_mips);

		// based on combination of layers and mipmaps, extract sub image information
		auto sub_img_rng = vw::cartesian_product(layer_rng, mipmap_rng) // cartesian product will produce a pair-wise range
		                 | vw::transform([&](auto &&layer_mip_pair) -> image_data::sub_image {
			auto [layer_idx, mip_idx] = layer_mip_pair;

			auto sub_data = ddsktx_sub_data{};
			ddsktx_get_sub(
				&image_data_header,
				&sub_data,
				image_data_ptr,
				image_data_size,
				layer_idx,
				0,
				mip_idx);

			auto sub_offset = uintptr_t(sub_data.buff) -
			                  uintptr_t(image_data_ptr) -
			                  uintptr_t(image_data_header.data_offset);

			return {
				.layer_index  = static_cast<uint32_t>(layer_idx),
				.mipmap_index = static_cast<uint32_t>(mip_idx),
				.offset       = sub_offset,
				.width        = static_cast<uint32_t>(sub_data.width),
				.height       = static_cast<uint32_t>(sub_data.height),
			};
		});

		// evaluate above defined range to make it "concrete"
		auto sub_images = sub_img_rng | rg::to<std::vector>();

		// remove header information from image data
		auto start_itr = std::begin(image_file_data);
		image_file_data.erase(start_itr, start_itr + image_data_header.data_offset);
		image_file_data.shrink_to_fit();

		// convert ddsktx header info to image_header
		auto image_header = image_data::image_header{
			.format       = to_sdl_format(image_data_header.format),
			.width        = static_cast<uint32_t>(image_data_header.width),
			.height       = static_cast<uint32_t>(image_data_header.height),
			.depth        = static_cast<uint32_t>(image_data_header.depth),
			.layer_count  = static_cast<uint32_t>(image_data_header.num_layers),
			.mipmap_count = static_cast<uint32_t>(image_data_header.num_mips),
		};

		return {
			.header     = image_header,
			.sub_images = sub_images,
			.data       = image_file_data,
		};
	}
}

/*
 * SDL base objects initializations
 */
namespace base
{
	// Compilation mode
	constexpr auto debug = bool{ _DEBUG };

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

		// get available GPU, D3D12 or Vulkan
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
	using sdl_free_texture      = sdl_gpu_deleter<SDL_ReleaseGPUTexture>;
	using sdl_gpu_texture_ptr   = std::unique_ptr<SDL_GPUTexture, sdl_free_texture>;

	// Structure to hold objects required to draw a frame
	struct frame_context
	{
		sdl_gfx_pipeline_ptr pipeline;

		sdl_gpu_buffer_ptr vertex_buffer;
		sdl_gpu_buffer_ptr index_buffer;
		uint32_t vertex_count;
		uint32_t index_count;

		sdl_gpu_texture_ptr grid_texture;
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
			.entrypoint = "main", // Assume shader's entry point is always main
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
		auto device = ctx.gpu.get();

		msg::info("Creating Pipelines.");

		auto vs_bin = io::read_file("shaders/instanced_shapes.vs_6_4.cso");
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
			  .format = SDL_GetGPUSwapchainTextureFormat(device, ctx.window.get()),
			}
		};

		auto pipeline_info = SDL_GPUGraphicsPipelineCreateInfo{
			.vertex_shader      = vs_shdr.get(),
			.fragment_shader    = fs_shdr.get(),
			.vertex_input_state = vertex_input,
			.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,

			.rasterizer_state = {
			  .fill_mode  = SDL_GPU_FILLMODE_FILL,
			  .cull_mode  = SDL_GPU_CULLMODE_NONE,
			  .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			},

			.target_info = {
			  .color_target_descriptions = color_targets.data(),
			  .num_color_targets         = static_cast<uint32_t>(color_targets.size()),
			},
		};

		auto pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
		msg::error(pipeline != nullptr, "Failed to create masker pipeline.");

		rndr.pipeline = sdl_gfx_pipeline_ptr(pipeline, sdl_free_gfx_pipeline{ device });
	}

	// Create Vertex Buffer, and using a Transfer Buffer upload vertex data
	void create_and_copy_vertices_indicies(const base::sdl_context &ctx,
	                                       const io::byte_span vertices,
	                                       const io::byte_span indicies,
	                                       frame_context &rndr)
	{
		auto device = ctx.gpu.get();

		msg::info("Create Vertex Buffer and Index Buffer.");

		auto vb_size = static_cast<uint32_t>(vertices.size());
		auto ib_size = static_cast<uint32_t>(indicies.size());

		auto vb_info = SDL_GPUBufferCreateInfo{
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size  = vb_size,
		};

		auto vertex_buffer = SDL_CreateGPUBuffer(device, &vb_info);
		msg::error(vertex_buffer != nullptr, "Could not create GPU Vertex Buffer.");
		SDL_SetGPUBufferName(device, vertex_buffer, "Vertex Buffer");
		rndr.vertex_buffer = sdl_gpu_buffer_ptr(vertex_buffer, sdl_free_buffer{ device });

		auto ib_info = SDL_GPUBufferCreateInfo{
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size  = ib_size,
		};

		auto index_buffer = SDL_CreateGPUBuffer(device, &ib_info);
		msg::error(index_buffer != nullptr, "Could not create GPU Vertex Buffer.");
		SDL_SetGPUBufferName(device, index_buffer, "Index Buffer");
		rndr.index_buffer = sdl_gpu_buffer_ptr(index_buffer, sdl_free_buffer{ device });

		msg::info("Create Transfer Buffer.");
		auto tb_size = vb_size + ib_size;

		auto transfer_buffer_info = SDL_GPUTransferBufferCreateInfo{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size  = tb_size,
		};

		auto transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_buffer_info);
		msg::error(transfer_buffer != nullptr, "Could not create GPU Transfer Buffer");

		msg::info("Upload vertices and indicies to Transfer Buffer.");
		auto *data = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);

		std::memcpy(data, vertices.data(), vertices.size());
		std::memcpy(io::offset_ptr(data, vb_size), indicies.data(), indicies.size());

		SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

		msg::info("Copy from Transfer Buffer to Vertex Buffer and Index Buffer.");
		auto copy_cmd = SDL_AcquireGPUCommandBuffer(device);
		auto copypass = SDL_BeginGPUCopyPass(copy_cmd);

		auto src = SDL_GPUTransferBufferLocation{
			.transfer_buffer = transfer_buffer,
			.offset          = 0,
		};
		auto dst = SDL_GPUBufferRegion{
			.buffer = vertex_buffer,
			.offset = 0,
			.size   = vb_size,
		};

		SDL_UploadToGPUBuffer(copypass, &src, &dst, false);

		src.offset = vb_size;

		dst = SDL_GPUBufferRegion{
			.buffer = index_buffer,
			.offset = 0,
			.size   = ib_size,
		};

		SDL_UploadToGPUBuffer(copypass, &src, &dst, false);

		SDL_EndGPUCopyPass(copypass);
		SDL_SubmitGPUCommandBuffer(copy_cmd);
		SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
	}

	void create_and_load_texture(const base::sdl_context &ctx, const io::image_data &texture_image, frame_context &rndr)
	{
		auto device = ctx.gpu.get();

		msg::info("Create GPU Texture.");

		auto texture_info = SDL_GPUTextureCreateInfo{
			.type                 = SDL_GPU_TEXTURETYPE_2D,
			.format               = texture_image.header.format,
			.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.width                = texture_image.header.width,
			.height               = texture_image.header.height,
			.layer_count_or_depth = texture_image.header.layer_count,
			.num_levels           = texture_image.header.mipmap_count,
		};
		auto texture = SDL_CreateGPUTexture(device, &texture_info);
		msg::error(texture != nullptr, "Failed to create GPU Texture");
		rndr.grid_texture = sdl_gpu_texture_ptr(texture, sdl_free_texture{ device });
		SDL_SetGPUTextureName(device, texture, "Sampler Texture");
		msg::info("Upload texture data to transfer buffer.");

		auto transfer_buffer_info = SDL_GPUTransferBufferCreateInfo{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size  = static_cast<uint32_t>(texture_image.data.size()),
		};
		auto transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_buffer_info);
		msg::error(transfer_buffer != nullptr, "Could not create GPU transfer buffer.");

		auto data = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
		std::memcpy(data, texture_image.data.data(), texture_image.data.size());
		SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

		msg::info("Copy from transfer buffer to texture buffer");
		auto copy_cmd = SDL_AcquireGPUCommandBuffer(device);
		auto copypass = SDL_BeginGPUCopyPass(copy_cmd);

		for (auto &&sub_image : texture_image.sub_images)
		{
			auto src = SDL_GPUTextureTransferInfo{
				.transfer_buffer = transfer_buffer,
				.offset          = static_cast<uint32_t>(sub_image.offset),
			};

			auto dst = SDL_GPUTextureRegion{
				.texture   = texture,
				.mip_level = sub_image.mipmap_index,
				.layer     = sub_image.layer_index,
				.w         = sub_image.width,
				.h         = sub_image.height,
				.d         = 1,
			};

			SDL_UploadToGPUTexture(copypass, &src, &dst, false);
		}

		SDL_EndGPUCopyPass(copypass);
		SDL_SubmitGPUCommandBuffer(copy_cmd);
		SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
	}

	// Initialize all Frame objects
	auto init(const base::sdl_context &ctx,
	          const io::byte_span vertices,
	          const io::byte_span indicies,
	          uint32_t vertex_count, uint32_t index_count,
	          const std::span<const SDL_GPUVertexAttribute> vertex_attributes,
	          const io::image_data &texture_image) -> frame_context
	{
		msg::info("Initialize frame objects");

		auto rndr = frame_context{
			.vertex_count = vertex_count,
			.index_count  = index_count,
		};

		create_pipelines(ctx, static_cast<uint32_t>(vertices.size() / vertex_count), vertex_attributes, rndr);
		create_and_copy_vertices_indicies(ctx, vertices, indicies, rndr);
		create_and_load_texture(ctx, texture_image, rndr);

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
			.clear_color = SDL_FColor{ 0.4f, 0.3f, 0.5f, 1.0f },
			.load_op     = SDL_GPU_LOADOP_CLEAR,
			.store_op    = SDL_GPU_STOREOP_STORE,
		};

		auto renderpass = SDL_BeginGPURenderPass(
			cmd_buf,
			&color_target_info,
			1,
			NULL);

		auto vertex_bindings = SDL_GPUBufferBinding{
			.buffer = rndr.vertex_buffer.get(),
			.offset = 0,
		};
		auto index_bindings = SDL_GPUBufferBinding{
			.buffer = rndr.index_buffer.get(),
			.offset = 0,
		};

		SDL_BindGPUVertexBuffers(renderpass, 0, &vertex_bindings, 1);
		SDL_BindGPUIndexBuffer(renderpass, &index_bindings, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		SDL_BindGPUGraphicsPipeline(renderpass, rndr.pipeline.get());
		SDL_DrawGPUIndexedPrimitives(renderpass,
		                             rndr.index_count,
		                             16,
		                             0,
		                             0,
		                             0);

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

	auto shape = std::array{
		app::pos_clr_vertex{ -1.f, -1.f, 0.f, 0, 0, 255, 255 },
		app::pos_clr_vertex{ 1.f, -1.f, 0.f, 0, 255, 0, 255 },
		app::pos_clr_vertex{ 1.f, 1.f, 0.f, 255, 0, 0, 255 },
		app::pos_clr_vertex{ -1.f, 1.f, 0.f, 255, 255, 0, 255 },
	};
	auto shape_indices = std::array{
		0, 1, 2,
		0, 2, 3
	};

	auto vertex_count = static_cast<uint32_t>(shape.size());
	auto index_count  = static_cast<uint32_t>(shape_indices.size());

	auto grid_texture = io::read_image_file("data/uv_grid.dds");

	auto rndr = frame::init(ctx,
	                        io::as_byte_span(shape),
	                        io::as_byte_span(shape_indices),
	                        vertex_count, index_count,
	                        app::pos_clr_vertex::vertex_attributes,
	                        grid_texture);

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

		frame::draw(ctx, rndr);
	}

	frame::destroy(ctx, rndr);

	base::destroy(ctx);

	return 0;
}