// For assert macro
#include <cassert>

// SDL 3 header
#include <SDL3/SDL.h>

// DDS and KTX texture file loader
// DDSKTX_IMPLEMENT must be defined in exactly one translation unit
// doesn't matter for this example, but it's good practice to define it in the same file as the implementation
#define DDSKTX_IMPLEMENT
#include <dds-ktx.h>

// GLM configuration and headers
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // GLM clip space should be in Z-axis to 0 to 1
#define GLM_FORCE_LEFT_HANDED       // GLM should use left-handed coordinates, +z goes into screen
#define GLM_FORCE_RADIANS           // GLM should always use radians not degrees.
#include <glm/glm.hpp>              // Required for glm::vec3/4/mat4/etc
#include <glm/ext.hpp>              // Required for glm::perspective function

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
	using sdl_free_sampler      = sdl_gpu_deleter<SDL_ReleaseGPUSampler>;
	using sdl_gpu_sampler_ptr   = std::unique_ptr<SDL_GPUSampler, sdl_free_sampler>;

	// Structure to hold objects required to draw a frame
	struct frame_context
	{
		sdl_gpu_texture_ptr depth_texture;

		sdl_gfx_pipeline_ptr pipeline;

		sdl_gpu_buffer_ptr vertex_buffer;
		sdl_gpu_buffer_ptr index_buffer;
		sdl_gpu_buffer_ptr instance_buffer;
		uint32_t vertex_count;
		uint32_t index_count;
		uint32_t instance_count;

		sdl_gpu_texture_ptr grid_texture;
		std::array<sdl_gpu_sampler_ptr, 6> samplers;
		uint8_t active_sampler = 0;

		io::byte_span view_proj;
	};

	// Create GPU side shader using in-memory shader binary for specified stage
	auto load_gpu_shader(const base::sdl_context &ctx,
	                     const io::byte_span &bin,
	                     SDL_GPUShaderStage stage,
	                     uint32_t sampler_count,
	                     uint32_t uniform_buffer_count,
	                     uint32_t storage_buffer_count,
	                     uint32_t storage_texture_count) -> sdl_gpu_shader_ptr
	{
		auto shader_format = [&]() -> SDL_GPUShaderFormat {
			auto backend_formats = SDL_GetGPUShaderFormats(ctx.gpu.get());

			if (backend_formats & SDL_GPU_SHADERFORMAT_DXIL)
				return SDL_GPU_SHADERFORMAT_DXIL;
			else
				return SDL_GPU_SHADERFORMAT_SPIRV;
		}();

		auto shader_info = SDL_GPUShaderCreateInfo{
			.code_size            = bin.size(),
			.code                 = reinterpret_cast<const std::uint8_t *>(bin.data()),
			.entrypoint           = "main", // Assume shader's entry point is always main
			.format               = shader_format,
			.stage                = stage,
			.num_samplers         = sampler_count,
			.num_storage_textures = storage_texture_count,
			.num_storage_buffers  = storage_buffer_count,
			.num_uniform_buffers  = uniform_buffer_count,
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

		auto vs_bin = io::read_file("shaders/instanced_mesh.vs_6_4.cso");
		auto fs_bin = io::read_file("shaders/textured_quad.ps_6_4.cso");

		auto vs_shdr = load_gpu_shader(ctx, vs_bin, SDL_GPU_SHADERSTAGE_VERTEX, 0, 1, 0, 0);
		auto fs_shdr = load_gpu_shader(ctx, fs_bin, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 0);

		auto vertex_description = SDL_GPUVertexBufferDescription{
			.slot               = 0,
			.pitch              = vertex_pitch,
			.input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX,
			.instance_step_rate = 0,
		};

		auto instance_description = SDL_GPUVertexBufferDescription{
			.slot               = 1,
			.pitch              = sizeof(glm::mat4),
			.input_rate         = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
			.instance_step_rate = 1,
		};

		auto vertex_buffer_descriptions = std::array{
			vertex_description,
			instance_description,
		};

		auto vertex_input = SDL_GPUVertexInputState{
			.vertex_buffer_descriptions = vertex_buffer_descriptions.data(),
			.num_vertex_buffers         = static_cast<uint32_t>(vertex_buffer_descriptions.size()),
			.vertex_attributes          = vertex_attributes.data(),
			.num_vertex_attributes      = static_cast<uint32_t>(vertex_attributes.size()),
		};

		auto color_targets = std::array{
			SDL_GPUColorTargetDescription{
			  .format = SDL_GetGPUSwapchainTextureFormat(device, ctx.window.get()),
			}
		};

		auto depth_stencil = SDL_GPUDepthStencilState{
			.compare_op          = SDL_GPU_COMPAREOP_LESS,
			.write_mask          = 0xff,
			.enable_depth_test   = true,
			.enable_depth_write  = true,
			.enable_stencil_test = false,
		};

		auto pipeline_info = SDL_GPUGraphicsPipelineCreateInfo{
			.vertex_shader      = vs_shdr.get(),
			.fragment_shader    = fs_shdr.get(),
			.vertex_input_state = vertex_input,
			.primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,

			.rasterizer_state = {
			  .fill_mode  = SDL_GPU_FILLMODE_FILL,
			  .cull_mode  = SDL_GPU_CULLMODE_BACK,
			  .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			},

			.depth_stencil_state = depth_stencil,

			.target_info = {
			  .color_target_descriptions = color_targets.data(),
			  .num_color_targets         = static_cast<uint32_t>(color_targets.size()),
			  .depth_stencil_format      = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
			  .has_depth_stencil_target  = true,
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

	void create_instance_buffer(const base::sdl_context &ctx, uint32_t instance_buffer_size, frame_context &rndr)
	{
		auto device = ctx.gpu.get();

		msg::info("Create Instance Buffer.");

		auto ib_info = SDL_GPUBufferCreateInfo{
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size  = instance_buffer_size,
		};
		auto instance_buffer = SDL_CreateGPUBuffer(device, &ib_info);
		msg::error(instance_buffer != nullptr, "Failed to create instance buffer.");
		rndr.instance_buffer = sdl_gpu_buffer_ptr(instance_buffer, sdl_free_buffer{ device });
	}

	void create_depth_texture(const base::sdl_context &ctx, frame_context &rndr)
	{
		auto device = ctx.gpu.get();

		int width = 0, height = 0;
		SDL_GetWindowSizeInPixels(ctx.window.get(), &width, &height);

		msg::info("Create Depth Stencil Texture");

		auto texture_info = SDL_GPUTextureCreateInfo{
			.type                 = SDL_GPU_TEXTURETYPE_2D,
			.format               = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
			.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
			.width                = static_cast<uint32_t>(width),
			.height               = static_cast<uint32_t>(height),
			.layer_count_or_depth = 1,
			.num_levels           = 1,
			.sample_count         = SDL_GPU_SAMPLECOUNT_1,
		};

		auto depth_texture = SDL_CreateGPUTexture(device, &texture_info);
		msg::error(depth_texture != nullptr, "Failed to create depth texture.");
		rndr.depth_texture = sdl_gpu_texture_ptr(depth_texture, sdl_free_texture{ device });
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

		// Copy data for each layer+mipmap in the array
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

	void create_samplers(const base::sdl_context &ctx, frame_context &rndr)
	{
		auto make_sampler = [](SDL_GPUDevice *device, const SDL_GPUSamplerCreateInfo &sampler_info) -> sdl_gpu_sampler_ptr {
			auto sampler = SDL_CreateGPUSampler(device, &sampler_info);
			msg::error(sampler != nullptr, "Failed to create sampler.");
			return sdl_gpu_sampler_ptr(sampler, sdl_free_sampler{ device });
		};

		auto device = ctx.gpu.get();

		msg::info("Create Point, Linear and Anisotropic; Clamp and Wrap Samplers");

		rndr.samplers[0] = make_sampler(device, /* Point Clamp */
		                                SDL_GPUSamplerCreateInfo{
										  .min_filter        = SDL_GPU_FILTER_NEAREST,
										  .mag_filter        = SDL_GPU_FILTER_NEAREST,
										  .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
										  .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .max_anisotropy    = 0,
										  .enable_anisotropy = false,
										});
		rndr.samplers[1] = make_sampler(device, /* Point Wrap */
		                                SDL_GPUSamplerCreateInfo{
										  .min_filter        = SDL_GPU_FILTER_NEAREST,
										  .mag_filter        = SDL_GPU_FILTER_NEAREST,
										  .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
										  .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .max_anisotropy    = 0,
										  .enable_anisotropy = false,
										});
		rndr.samplers[2] = make_sampler(device, /* Linear Clamp */
		                                SDL_GPUSamplerCreateInfo{
										  .min_filter        = SDL_GPU_FILTER_LINEAR,
										  .mag_filter        = SDL_GPU_FILTER_LINEAR,
										  .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
										  .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .max_anisotropy    = 0,
										  .enable_anisotropy = false,
										});
		rndr.samplers[3] = make_sampler(device, /* Linear Wrap */
		                                SDL_GPUSamplerCreateInfo{
										  .min_filter        = SDL_GPU_FILTER_LINEAR,
										  .mag_filter        = SDL_GPU_FILTER_LINEAR,
										  .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
										  .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .max_anisotropy    = 0,
										  .enable_anisotropy = false,
										});
		rndr.samplers[4] = make_sampler(device, /* Anisotropic Clamp */
		                                SDL_GPUSamplerCreateInfo{
										  .min_filter        = SDL_GPU_FILTER_LINEAR,
										  .mag_filter        = SDL_GPU_FILTER_LINEAR,
										  .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
										  .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
										  .max_anisotropy    = 4,
										  .enable_anisotropy = true,
										});
		rndr.samplers[5] = make_sampler(device, /* Anisotropic Wrap */
		                                SDL_GPUSamplerCreateInfo{
										  .min_filter        = SDL_GPU_FILTER_LINEAR,
										  .mag_filter        = SDL_GPU_FILTER_LINEAR,
										  .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
										  .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
										  .max_anisotropy    = 4,
										  .enable_anisotropy = true,
										});

		rndr.active_sampler = 5;
	}

	void update_instance_buffer(const base::sdl_context &ctx, const io::byte_span instances, frame_context &rndr)
	{
		auto device = ctx.gpu.get();

		msg::info("Update instance buffer");

		auto ib_size = static_cast<uint32_t>(instances.size());

		auto tb_info = SDL_GPUTransferBufferCreateInfo{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size  = ib_size,
		};
		auto transfer_buffer = SDL_CreateGPUTransferBuffer(device, &tb_info);
		msg::error(transfer_buffer != nullptr, "Failed to create transfer buffer for instance data.");

		auto *data = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
		std::memcpy(data, instances.data(), ib_size);
		SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

		auto copy_cmd = SDL_AcquireGPUCommandBuffer(device);
		auto copypass = SDL_BeginGPUCopyPass(copy_cmd);

		auto src = SDL_GPUTransferBufferLocation{
			.transfer_buffer = transfer_buffer,
			.offset          = 0,
		};
		auto dst = SDL_GPUBufferRegion{
			.buffer = rndr.instance_buffer.get(),
			.offset = 0,
			.size   = ib_size,
		};
		SDL_UploadToGPUBuffer(copypass, &src, &dst, false);

		SDL_EndGPUCopyPass(copypass);
		SDL_SubmitGPUCommandBuffer(copy_cmd);
		SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
	}

	// Initialize all Frame objects
	auto init(const base::sdl_context &ctx,
	          const io::byte_span vertices,
	          const io::byte_span indicies,
	          const io::byte_span instances,
	          uint32_t vertex_count, uint32_t index_count, uint32_t instance_count,
	          const std::span<const SDL_GPUVertexAttribute> vertex_attributes,
	          const io::image_data &texture_image,
	          const io::byte_span &view_proj) -> frame_context
	{
		msg::info("Initialize frame objects");

		auto rndr = frame_context{
			.vertex_count   = vertex_count,
			.index_count    = index_count,
			.instance_count = instance_count,
			.view_proj      = view_proj,
		};

		create_pipelines(ctx, static_cast<uint32_t>(vertices.size() / vertex_count), vertex_attributes, rndr);
		create_and_copy_vertices_indicies(ctx, vertices, indicies, rndr);
		create_instance_buffer(ctx, static_cast<uint32_t>(instances.size()), rndr);
		create_and_load_texture(ctx, texture_image, rndr);
		create_depth_texture(ctx, rndr);
		create_samplers(ctx, rndr);

		update_instance_buffer(ctx, instances, rndr);

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

		auto depth_target_info = SDL_GPUDepthStencilTargetInfo{
			.texture          = rndr.depth_texture.get(),
			.clear_depth      = 1,
			.load_op          = SDL_GPU_LOADOP_CLEAR,
			.store_op         = SDL_GPU_STOREOP_STORE,
			.stencil_load_op  = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_STORE,
			.cycle            = true,
			.clear_stencil    = 0,
		};

		auto renderpass = SDL_BeginGPURenderPass(
			cmd_buf,
			&color_target_info,
			1,
			&depth_target_info);

		SDL_PushGPUVertexUniformData(cmd_buf, 0, rndr.view_proj.data(), static_cast<uint32_t>(rndr.view_proj.size()));

		auto vertex_bindings = std::array{
			SDL_GPUBufferBinding{
			  .buffer = rndr.vertex_buffer.get(),
			  .offset = 0,
			},
			SDL_GPUBufferBinding{
			  .buffer = rndr.instance_buffer.get(),
			  .offset = 0,
			},
		};
		SDL_BindGPUVertexBuffers(renderpass, 0, vertex_bindings.data(), static_cast<uint32_t>(vertex_bindings.size()));

		auto index_bindings = SDL_GPUBufferBinding{
			.buffer = rndr.index_buffer.get(),
			.offset = 0,
		};
		SDL_BindGPUIndexBuffer(renderpass, &index_bindings, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		auto sampler_binding = SDL_GPUTextureSamplerBinding{
			.texture = rndr.grid_texture.get(),
			.sampler = rndr.samplers[rndr.active_sampler].get(),
		};
		SDL_BindGPUFragmentSamplers(renderpass, 0, &sampler_binding, 1);

		SDL_BindGPUGraphicsPipeline(renderpass, rndr.pipeline.get());
		SDL_DrawGPUIndexedPrimitives(renderpass,
		                             rndr.index_count,
		                             rndr.instance_count,
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
	struct pos_uv_vertex
	{
		glm::vec3 pos;
		glm::vec2 uv;
	};
	struct instance_data
	{
		glm::mat4 transform;
	};

	struct mesh
	{
		std::vector<pos_uv_vertex> vertices;
		std::vector<uint32_t> indices;
	};

	auto make_cube() -> mesh
	{
		auto vertices = std::vector<pos_uv_vertex>{
			// +X face
			{ { 1.f, -1.f, -1.f }, { 0.f, 1.f } },
			{ { 1.f, -1.f, +1.f }, { 0.f, 0.f } },
			{ { 1.f, +1.f, +1.f }, { 1.f, 0.f } },
			{ { 1.f, +1.f, -1.f }, { 1.f, 1.f } },
			// -X face
			{ { -1.f, -1.f, -1.f }, { 0.f, 1.f } },
			{ { -1.f, +1.f, -1.f }, { 1.f, 1.f } },
			{ { -1.f, +1.f, +1.f }, { 1.f, 0.f } },
			{ { -1.f, -1.f, +1.f }, { 0.f, 0.f } },
			// +Y face
			{ { -1.f, 1.f, -1.f }, { 0.f, 1.f } },
			{ { +1.f, 1.f, -1.f }, { 1.f, 1.f } },
			{ { +1.f, 1.f, +1.f }, { 1.f, 0.f } },
			{ { -1.f, 1.f, +1.f }, { 0.f, 0.f } },
			// -Y face
			{ { -1.f, -1.f, -1.f }, { 0.f, 1.f } },
			{ { -1.f, -1.f, +1.f }, { 0.f, 0.f } },
			{ { +1.f, -1.f, +1.f }, { 1.f, 0.f } },
			{ { +1.f, -1.f, -1.f }, { 1.f, 1.f } },
			// +Z face
			{ { -1.f, -1.f, 1.f }, { 0.f, 1.f } },
			{ { -1.f, +1.f, 1.f }, { 0.f, 0.f } },
			{ { +1.f, +1.f, 1.f }, { 1.f, 0.f } },
			{ { +1.f, -1.f, 1.f }, { 1.f, 1.f } },
			// -Z face
			{ { -1.f, -1.f, -1.f }, { 0.f, 1.f } },
			{ { +1.f, -1.f, -1.f }, { 1.f, 1.f } },
			{ { +1.f, +1.f, -1.f }, { 1.f, 0.f } },
			{ { -1.f, +1.f, -1.f }, { 0.f, 0.f } },
		};

		auto face_0 = std::vector<uint32_t>{
			0, 1, 2, //
			0, 2, 3, //
		};
		auto f_idxs = std::views::iota(0, 6);
		auto f_rng  = std::views::cartesian_product(f_idxs, face_0) |
		             std::views::transform([](const auto &&f_pair) -> uint32_t {
			auto [f_idx, v_idx] = f_pair;

			return v_idx + (f_idx * 4);
		});

		auto indices = f_rng | std::ranges::to<std::vector>();

		return {
			vertices,
			indices,
		};
	}

	auto get_cube_instances() -> std::vector<instance_data>
	{
		auto cube_1 = glm::translate(glm::mat4(1.f), glm::vec3{ 2.f, 0.f, 0.f });

		auto cube_2 = glm::translate(glm::mat4(1.0f), glm::vec3{ -2.f, 0.f, 0.f });
		cube_2      = glm::rotate(cube_2, glm::radians(45.0f), glm::vec3{ 0.f, 0.f, 1.f });

		return {
			{ cube_1 },
			{ cube_2 },
		};
	}

	auto get_projection(uint32_t width, uint32_t height) -> glm::mat4
	{
		auto fov          = glm::radians(90.0f);
		auto aspect_ratio = static_cast<float>(width) / height;

		auto projection = glm::perspective(fov, aspect_ratio, 0.f, 100.f);
		auto view       = glm::lookAt(glm::vec3(0.f, 1.5f, -2.5f),
		                              glm::vec3(0.f, 0.f, 0.f),
		                              glm::vec3(0.f, 1.f, 0.f));

		return projection * view;
	}

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
		  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		  .offset      = sizeof(glm::vec3),
		},
		SDL_GPUVertexAttribute{
		  .location    = 2,
		  .buffer_slot = 1,
		  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		  .offset      = 0,
		},
		SDL_GPUVertexAttribute{
		  .location    = 3,
		  .buffer_slot = 1,
		  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		  .offset      = sizeof(glm::vec4),
		},
		SDL_GPUVertexAttribute{
		  .location    = 4,
		  .buffer_slot = 1,
		  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		  .offset      = sizeof(glm::vec4) * 2,
		},
		SDL_GPUVertexAttribute{
		  .location    = 5,
		  .buffer_slot = 1,
		  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		  .offset      = sizeof(glm::vec4) * 3,
		},
	};

	void update(frame::frame_context &rndr)
	{
		auto *key_states = SDL_GetKeyboardState(NULL);

		auto prv = rndr.active_sampler;

		if (key_states[SDL_SCANCODE_1])
			rndr.active_sampler = 0;
		else if (key_states[SDL_SCANCODE_2])
			rndr.active_sampler = 1;
		else if (key_states[SDL_SCANCODE_3])
			rndr.active_sampler = 2;
		else if (key_states[SDL_SCANCODE_4])
			rndr.active_sampler = 3;
		else if (key_states[SDL_SCANCODE_5])
			rndr.active_sampler = 4;
		else if (key_states[SDL_SCANCODE_6])
			rndr.active_sampler = 5;

		if (prv != rndr.active_sampler)
		{
			constexpr auto sampler_name = std::array{
				"Point Clamp"sv,
				"Point Wrap"sv,
				"Linear Clamp"sv,
				"Linear Wrap"sv,
				"Anisotropic Clamp"sv,
				"Anisotropic Wrap"sv,
			};
			msg::info(std::format("Change sampler to {}", sampler_name.at(rndr.active_sampler)));
		}
	}
}

auto main() -> int
{
	constexpr auto app_title     = "SDL3 GPU minimal example."sv;
	constexpr auto window_width  = 1920;
	constexpr auto window_height = 1080;

	auto ctx = base::init(window_width, window_height, app_title);

	auto shape           = app::make_cube();
	auto shape_instances = app::get_cube_instances();
	auto view_proj       = app::get_projection(window_width, window_height);

	auto vertex_count   = static_cast<uint32_t>(shape.vertices.size());
	auto index_count    = static_cast<uint32_t>(shape.indices.size());
	auto instance_count = static_cast<uint32_t>(shape_instances.size());

	auto grid_texture = io::read_image_file("data/uv_grid.dds");

	auto rndr = frame::init(ctx,
	                        io::as_byte_span(shape.vertices),
	                        io::as_byte_span(shape.indices),
	                        io::as_byte_span(shape_instances),
	                        vertex_count, index_count, instance_count,
	                        app::vertex_attributes,
	                        grid_texture,
	                        io::as_byte_span(view_proj));

	grid_texture = {}; // don't need it once data is uploaded to gpu

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
		app::update(rndr);

		frame::draw(ctx, rndr);
	}

	frame::destroy(ctx, rndr);

	base::destroy(ctx);

	return 0;
}