module;

// SDL 3 header
#include <SDL3/SDL.h>

export module sdl3_scene;

import std;
import logs;
import io;
import sdl3_init;

// literal suffixes for strings, string_view, etc
using namespace std::literals;

export namespace sdl3
{
	constexpr auto DEPTH_FORMAT   = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
	constexpr auto MAX_ANISOTROPY = float{ 16 };
	constexpr auto MSAA           = SDL_GPU_SAMPLECOUNT_1;

	// Deleter template, for use with SDL objects.
	// Allows use of SDL Objects with C++'s smart pointers, using SDL's destroy function
	// This version needs pointer to GPU.
	template <auto fn>
	struct gpu_deleter
	{
		SDL_GPUDevice *gpu = nullptr;
		constexpr void operator()(auto *arg)
		{
			fn(gpu, arg);
		}
	};
	// Typedefs for SDL objects that need GPU Device to properly destruct
	using free_gfx_pipeline = gpu_deleter<SDL_ReleaseGPUGraphicsPipeline>;
	using gfx_pipeline_ptr  = std::unique_ptr<SDL_GPUGraphicsPipeline, free_gfx_pipeline>;
	using free_gfx_shader   = gpu_deleter<SDL_ReleaseGPUShader>;
	using gpu_shader_ptr    = std::unique_ptr<SDL_GPUShader, free_gfx_shader>;
	using free_buffer       = gpu_deleter<SDL_ReleaseGPUBuffer>;
	using gpu_buffer_ptr    = std::unique_ptr<SDL_GPUBuffer, free_buffer>;
	using free_texture      = gpu_deleter<SDL_ReleaseGPUTexture>;
	using gpu_texture_ptr   = std::unique_ptr<SDL_GPUTexture, free_texture>;
	using free_sampler      = gpu_deleter<SDL_ReleaseGPUSampler>;
	using gpu_sampler_ptr   = std::unique_ptr<SDL_GPUSampler, free_sampler>;

	struct shader_desc
	{
		io::byte_array shader_binary;
		SDL_GPUShaderStage stage;
		uint32_t sampler_count         = 0;
		uint32_t uniform_buffer_count  = 0;
		uint32_t storage_buffer_count  = 0;
		uint32_t storage_texture_count = 0;
	};

	auto make_gpu_shader(SDL_GPUDevice *gpu, const shader_desc &desc) -> gpu_shader_ptr
	{
		auto shader_format = [&]() -> SDL_GPUShaderFormat {
			auto backend_formats = SDL_GetGPUShaderFormats(gpu);

			if (backend_formats & SDL_GPU_SHADERFORMAT_DXIL)
				return SDL_GPU_SHADERFORMAT_DXIL;
			else
				return SDL_GPU_SHADERFORMAT_SPIRV;
		}();

		auto shader_info = SDL_GPUShaderCreateInfo{
			.code_size            = desc.shader_binary.size(),
			.code                 = reinterpret_cast<const uint8_t *>(desc.shader_binary.data()),
			.entrypoint           = "main",
			.format               = shader_format,
			.stage                = desc.stage,
			.num_samplers         = desc.sampler_count,
			.num_storage_textures = desc.storage_texture_count,
			.num_storage_buffers  = desc.storage_buffer_count,
			.num_uniform_buffers  = desc.uniform_buffer_count,
		};

		auto shader = SDL_CreateGPUShader(gpu, &shader_info);
		msg::error(shader != nullptr, "Failed to create shader.");

		return { shader, { gpu } };
	}

	enum class cull_mode_t
	{
		none,
		front_ccw,
		back_ccw,
		front_cw,
		back_cw,
	};

	struct pipeline_desc
	{
		shader_desc vertex;
		shader_desc fragment;

		std::span<const SDL_GPUVertexAttribute> vertex_attributes;
		std::span<const SDL_GPUVertexBufferDescription> vertex_buffer_descriptions;

		bool depth_test;
		cull_mode_t cull_mode = cull_mode_t::back_ccw;
	};

	auto make_gfx_pipeline(const context &ctx, const pipeline_desc &desc) -> gfx_pipeline_ptr
	{
		auto gpu = ctx.gpu.get();
		auto wnd = ctx.window.get();

		msg::info("Creating Pipelines.");

		auto vs_shdr = make_gpu_shader(gpu, desc.vertex);
		auto fs_shdr = make_gpu_shader(gpu, desc.fragment);

		auto vertex_input_state = SDL_GPUVertexInputState{
			.vertex_buffer_descriptions = desc.vertex_buffer_descriptions.data(),
			.num_vertex_buffers         = static_cast<uint32_t>(desc.vertex_buffer_descriptions.size()),
			.vertex_attributes          = desc.vertex_attributes.data(),
			.num_vertex_attributes      = static_cast<uint32_t>(desc.vertex_attributes.size()),
		};

		auto rasterizer_state = [&]() -> SDL_GPURasterizerState {
			switch (desc.cull_mode)
			{
				using cm = cull_mode_t;
			case cm::none:
				return {
					.fill_mode = SDL_GPU_FILLMODE_FILL,
					.cull_mode = SDL_GPU_CULLMODE_NONE,
				};
			case cm::front_ccw:
				return {
					.fill_mode  = SDL_GPU_FILLMODE_FILL,
					.cull_mode  = SDL_GPU_CULLMODE_FRONT,
					.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
				};
			case cm::back_ccw:
				return {
					.fill_mode  = SDL_GPU_FILLMODE_FILL,
					.cull_mode  = SDL_GPU_CULLMODE_BACK,
					.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
				};
			case cm::front_cw:
				return {
					.fill_mode  = SDL_GPU_FILLMODE_FILL,
					.cull_mode  = SDL_GPU_CULLMODE_FRONT,
					.front_face = SDL_GPU_FRONTFACE_CLOCKWISE
				};
			case cm::back_cw:
				return {
					.fill_mode  = SDL_GPU_FILLMODE_FILL,
					.cull_mode  = SDL_GPU_CULLMODE_BACK,
					.front_face = SDL_GPU_FRONTFACE_CLOCKWISE
				};
			}
			msg::error(false, "Unhandled Cull Mode case.");
			return {};
		}();

		auto depth_stencil_state = SDL_GPUDepthStencilState{};
		if (desc.depth_test)
		{
			depth_stencil_state = SDL_GPUDepthStencilState{
				.compare_op          = SDL_GPU_COMPAREOP_LESS,
				.write_mask          = std::numeric_limits<uint8_t>::max(),
				.enable_depth_test   = true,
				.enable_depth_write  = true,
				.enable_stencil_test = false,
			};
		}

		auto blend_state = SDL_GPUColorTargetBlendState{
			.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
			.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.color_blend_op        = SDL_GPU_BLENDOP_ADD,
			.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
			.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
			.alpha_blend_op        = SDL_GPU_BLENDOP_ADD,
			.enable_blend          = true,
		};

		auto color_targets = std::array{
			SDL_GPUColorTargetDescription{
			  .format      = SDL_GetGPUSwapchainTextureFormat(gpu, wnd),
			  .blend_state = blend_state,
			},
		};

		auto target_info = SDL_GPUGraphicsPipelineTargetInfo{
			.color_target_descriptions = color_targets.data(),
			.num_color_targets         = static_cast<uint32_t>(color_targets.size()),
			.depth_stencil_format      = DEPTH_FORMAT,
			.has_depth_stencil_target  = desc.depth_test,
		};

		auto pipeline_info = SDL_GPUGraphicsPipelineCreateInfo{
			.vertex_shader       = vs_shdr.get(),
			.fragment_shader     = fs_shdr.get(),
			.vertex_input_state  = vertex_input_state,
			.primitive_type      = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.rasterizer_state    = rasterizer_state,
			.depth_stencil_state = depth_stencil_state,
			.target_info         = target_info,
		};
		auto pl = SDL_CreateGPUGraphicsPipeline(gpu, &pipeline_info);
		msg::error(pl != nullptr, "Failed to create pipeline.");

		return { pl, { gpu } };
	}

	auto make_buffer(SDL_GPUDevice *gpu, SDL_GPUBufferUsageFlags usage, uint32_t size, std::string_view name = ""sv) -> gpu_buffer_ptr
	{
		msg::info(std::format("Create gpu buffer. {}/{}", usage, size));

		auto buffer_info = SDL_GPUBufferCreateInfo{
			.usage = usage,
			.size  = size,
		};

		auto buffer = SDL_CreateGPUBuffer(gpu, &buffer_info);
		msg::error(buffer != nullptr, "Failed to create gpu buffer");

		if (name.size() > 0)
		{
			SDL_SetGPUBufferName(gpu, buffer, name.data());
		}

		return { buffer, { gpu } };
	}

	struct texture_desc
	{
		SDL_GPUTextureUsageFlags usage;
		SDL_GPUTextureFormat format;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		uint32_t mip_levels;
		SDL_GPUSampleCount sample_count = MSAA;
	};

	auto make_texture(SDL_GPUDevice *gpu, const texture_desc &desc, std::string_view name = ""sv) -> gpu_texture_ptr
	{
		msg::info(std::format("Create gpu texture. {}x{}x{}", desc.width, desc.height, desc.depth));

		auto texture_info = SDL_GPUTextureCreateInfo{
			.type                 = SDL_GPU_TEXTURETYPE_2D,
			.format               = desc.format,
			.usage                = desc.usage,
			.width                = desc.width,
			.height               = desc.height,
			.layer_count_or_depth = desc.depth,
			.num_levels           = desc.mip_levels,
			.sample_count         = desc.sample_count,
		};

		auto texture = SDL_CreateGPUTexture(gpu, &texture_info);
		msg::error(texture != nullptr, "Failed to create gpu texture.");

		if (name.size() > 0)
		{
			SDL_SetGPUTextureName(gpu, texture, name.data());
		}

		return { texture, { gpu } };
	}

	enum class sampler_type
	{
		point_clamp,
		point_wrap,
		linear_clamp,
		linear_wrap,
		anisotropic_clamp,
		anisotropic_wrap,
	};

	auto to_string(sampler_type type) -> std::string_view
	{
		constexpr static auto type_names = std::array{
			"point_clamp"sv,
			"point_wrap"sv,
			"linear_clamp"sv,
			"linear_wrap"sv,
			"anisotropic_clamp"sv,
			"anisotropic_wrap"sv,
		};

		return type_names.at(static_cast<uint8_t>(type));
	}

	auto make_sampler(SDL_GPUDevice *gpu, sampler_type type) -> gpu_sampler_ptr
	{
		msg::info(std::format("Create gpu sampler. {}", to_string(type)));

		auto sampler_info = [&]() -> SDL_GPUSamplerCreateInfo {
			switch (type)
			{
			case sampler_type::point_clamp:
				return {
					.min_filter        = SDL_GPU_FILTER_NEAREST,
					.mag_filter        = SDL_GPU_FILTER_NEAREST,
					.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
					.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.max_anisotropy    = 0,
					.enable_anisotropy = false,
				};
			case sampler_type::point_wrap:
				return {
					.min_filter        = SDL_GPU_FILTER_NEAREST,
					.mag_filter        = SDL_GPU_FILTER_NEAREST,
					.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
					.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.max_anisotropy    = 0,
					.enable_anisotropy = false,
				};
			case sampler_type::linear_clamp:
				return {
					.min_filter        = SDL_GPU_FILTER_LINEAR,
					.mag_filter        = SDL_GPU_FILTER_LINEAR,
					.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
					.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.max_anisotropy    = 0,
					.enable_anisotropy = false,
				};
			case sampler_type::linear_wrap:
				return {
					.min_filter        = SDL_GPU_FILTER_LINEAR,
					.mag_filter        = SDL_GPU_FILTER_LINEAR,
					.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
					.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.max_anisotropy    = 0,
					.enable_anisotropy = false,
				};
			case sampler_type::anisotropic_clamp:
				return {
					.min_filter        = SDL_GPU_FILTER_LINEAR,
					.mag_filter        = SDL_GPU_FILTER_LINEAR,
					.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
					.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
					.max_anisotropy    = MAX_ANISOTROPY,
					.enable_anisotropy = true,
				};
			case sampler_type::anisotropic_wrap:
				return {
					.min_filter        = SDL_GPU_FILTER_LINEAR,
					.mag_filter        = SDL_GPU_FILTER_LINEAR,
					.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
					.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
					.max_anisotropy    = MAX_ANISOTROPY,
					.enable_anisotropy = true,
				};
			}

			return {};
		}();

		auto sampler = SDL_CreateGPUSampler(gpu, &sampler_info);
		msg::error(sampler != nullptr, "Failed to create sampler.");

		return { sampler, { gpu } };
	}

	struct scene
	{
		SDL_FColor clear_color;

		std::vector<gfx_pipeline_ptr> pipelines;

		gpu_buffer_ptr vertex_buffer;
		gpu_buffer_ptr index_buffer;
		gpu_buffer_ptr instance_buffer;
		uint32_t vertex_count;
		uint32_t index_count;
		uint32_t instance_count;

		gpu_texture_ptr depth_texture;
		gpu_texture_ptr uv_texture;
		gpu_sampler_ptr uv_sampler;

		io::byte_span view_projection;
	};

	void upload_to_gpu(SDL_GPUDevice *gpu,
	                   const io::byte_span vertices,
	                   const io::byte_span indices,
	                   const io::byte_span instances,
	                   const io::image_data &texture,
	                   scene &scn)
	{
		msg::info("Upload to gpu memory.");

		auto tb_size = static_cast<uint32_t>(vertices.size() + indices.size() + instances.size() + texture.data.size());

		auto transfer_info = SDL_GPUTransferBufferCreateInfo{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size  = tb_size,
		};
		auto transfer_buffer = SDL_CreateGPUTransferBuffer(gpu, &transfer_info);
		msg::error(transfer_buffer != nullptr, "Failed to create gpu transfer buffer.");

		auto data = SDL_MapGPUTransferBuffer(gpu, transfer_buffer, false);

		// vertices
		std::memcpy(data, vertices.data(), vertices.size());
		// indicies
		data = io::offset_ptr(data, vertices.size());
		std::memcpy(data, indices.data(), indices.size());
		// instances
		data = io::offset_ptr(data, indices.size());
		std::memcpy(data, instances.data(), instances.size());
		// texture
		data = io::offset_ptr(data, instances.size());
		std::memcpy(data, texture.data.data(), texture.data.size());

		SDL_UnmapGPUTransferBuffer(gpu, transfer_buffer);

		msg::info("Copy from gpu memory to gpu resources");
		auto copy_cmd  = SDL_AcquireGPUCommandBuffer(gpu);
		auto copy_pass = SDL_BeginGPUCopyPass(copy_cmd);

		auto offset = 0u;
		auto src_b  = SDL_GPUTransferBufferLocation{
			 .transfer_buffer = transfer_buffer,
			 .offset          = offset,
		};

		// vertices
		{
			auto dst = SDL_GPUBufferRegion{
				.buffer = scn.vertex_buffer.get(),
				.offset = 0,
				.size   = static_cast<uint32_t>(vertices.size()),
			};
			SDL_UploadToGPUBuffer(copy_pass, &src_b, &dst, false);
			offset += dst.size;
		}
		// indices
		{
			src_b.offset = offset;

			auto dst = SDL_GPUBufferRegion{
				.buffer = scn.index_buffer.get(),
				.offset = 0,
				.size   = static_cast<uint32_t>(indices.size()),
			};
			SDL_UploadToGPUBuffer(copy_pass, &src_b, &dst, false);
			offset += dst.size;
		}
		// instances
		{
			src_b.offset = offset;

			auto dst = SDL_GPUBufferRegion{
				.buffer = scn.instance_buffer.get(),
				.offset = 0,
				.size   = static_cast<uint32_t>(instances.size()),
			};
			SDL_UploadToGPUBuffer(copy_pass, &src_b, &dst, false);
			offset += dst.size;
		}
		// texture
		{
			// Copy data for each layer+mipmap in the array
			for (auto &&sub_image : texture.sub_images)
			{
				auto src_t = SDL_GPUTextureTransferInfo{
					.transfer_buffer = transfer_buffer,
					.offset          = offset + static_cast<uint32_t>(sub_image.offset),
				};

				auto dst = SDL_GPUTextureRegion{
					.texture   = scn.uv_texture.get(),
					.mip_level = sub_image.mipmap_index,
					.layer     = sub_image.layer_index,
					.w         = sub_image.width,
					.h         = sub_image.height,
					.d         = 1,
				};

				SDL_UploadToGPUTexture(copy_pass, &src_t, &dst, false);
			}
		}

		SDL_EndGPUCopyPass(copy_pass);
		SDL_SubmitGPUCommandBuffer(copy_cmd);
		SDL_ReleaseGPUTransferBuffer(gpu, transfer_buffer);
	}

	auto init_scene(const context &ctx,
	                const std::span<const pipeline_desc> pipelines,
	                const io::byte_span vertices, uint32_t vertex_count,
	                const io::byte_span indices, uint32_t index_count,
	                const io::byte_span instances, uint32_t instance_count,
	                const io::image_data &texture) -> scene
	{
		auto gpu = ctx.gpu.get();
		auto w = 0, h = 0;
		SDL_GetWindowSizeInPixels(ctx.window.get(), &w, &h);

		msg::info("Create Scene.");

		auto scn = scene{
			.vertex_count   = vertex_count,
			.index_count    = index_count,
			.instance_count = instance_count,
		};

		std::ranges::transform(pipelines, std::back_inserter(scn.pipelines), [&](const auto &pipeline) {
			return make_gfx_pipeline(ctx, pipeline);
		});
		scn.vertex_buffer   = make_buffer(gpu, SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<uint32_t>(vertices.size()), "Vertex Buffer"sv);
		scn.index_buffer    = make_buffer(gpu, SDL_GPU_BUFFERUSAGE_INDEX, static_cast<uint32_t>(indices.size()), "Index Buffer"sv);
		scn.instance_buffer = make_buffer(gpu, SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<uint32_t>(instances.size()), "Instance Buffer"sv);

		auto td = texture_desc{
			.usage      = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
			.format     = DEPTH_FORMAT,
			.width      = static_cast<uint32_t>(w),
			.height     = static_cast<uint32_t>(h),
			.depth      = 1,
			.mip_levels = 1,
		};
		scn.depth_texture = make_texture(gpu, td, "Depth Texture"sv);

		auto td2 = texture_desc{
			.usage      = SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.format     = texture.header.format,
			.width      = texture.header.width,
			.height     = texture.header.height,
			.depth      = texture.header.layer_count,
			.mip_levels = texture.header.mipmap_count,
		};
		scn.uv_texture = make_texture(gpu, td2, "UV texture"sv);
		scn.uv_sampler = make_sampler(gpu, sampler_type::anisotropic_clamp);

		upload_to_gpu(gpu, vertices, indices, instances, texture, scn);

		return scn;
	}

	void destroy_scene(scene &scn)
	{
		msg::info("Destroy Scene.");

		scn = {};
	}

	// Get Swapchain Image/Texture, wait if none is available
	// Does not use smart pointer as lifetime of swapchain texture is managed by SDL
	auto get_swapchain_texture(SDL_Window *wnd, SDL_GPUCommandBuffer *cmd_buf) -> SDL_GPUTexture *
	{
		auto sc_tex = (SDL_GPUTexture *)nullptr;

		auto res = SDL_WaitAndAcquireGPUSwapchainTexture(cmd_buf, wnd, &sc_tex, NULL, NULL);
		msg::error(res == true, "Wait and acquire GPU swapchain texture failed.");
		msg::error(sc_tex != nullptr, "Swapchain texture is null. Is window minimized?");

		return sc_tex;
	}

	void draw(const context &ctx, const scene &scn, const io::byte_span view_proj)
	{
		auto gpu = ctx.gpu.get();
		auto wnd = ctx.window.get();

		auto cmd_buf = SDL_AcquireGPUCommandBuffer(gpu);
		msg::error(cmd_buf != nullptr, "Failed to acquire command buffer");

		// Push Uniform buffer
		SDL_PushGPUVertexUniformData(cmd_buf, 0, view_proj.data(), static_cast<uint32_t>(view_proj.size()));

		// Swapchain image
		auto sc_img = get_swapchain_texture(wnd, cmd_buf);

		auto color_target = SDL_GPUColorTargetInfo{
			.texture     = sc_img,
			.clear_color = scn.clear_color,
			.load_op     = SDL_GPU_LOADOP_CLEAR,
			.store_op    = SDL_GPU_STOREOP_STORE,
		};

		auto depth_target = SDL_GPUDepthStencilTargetInfo{
			.texture          = scn.depth_texture.get(),
			.clear_depth      = 1.0f,
			.load_op          = SDL_GPU_LOADOP_CLEAR,
			.store_op         = SDL_GPU_STOREOP_STORE,
			.stencil_load_op  = SDL_GPU_LOADOP_CLEAR,
			.stencil_store_op = SDL_GPU_STOREOP_STORE,
			.cycle            = true,
			.clear_stencil    = 0,
		};

		auto render_pass = SDL_BeginGPURenderPass(cmd_buf, &color_target, 1, &depth_target);
		{
			// Vertex and Instance buffer
			auto vertex_bindings = std::array{
				SDL_GPUBufferBinding{
				  .buffer = scn.vertex_buffer.get(),
				  .offset = 0,
				},
				SDL_GPUBufferBinding{
				  .buffer = scn.instance_buffer.get(),
				  .offset = 0,
				},
			};
			SDL_BindGPUVertexBuffers(render_pass, 0, vertex_bindings.data(), static_cast<uint32_t>(vertex_bindings.size()));

			// Index Buffer
			auto index_binding = SDL_GPUBufferBinding{
				.buffer = scn.index_buffer.get(),
				.offset = 0,
			};
			SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

			// UV Texture and Sampler
			auto sampler_binding = SDL_GPUTextureSamplerBinding{
				.texture = scn.uv_texture.get(),
				.sampler = scn.uv_sampler.get(),
			};
			SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

			// Graphics Pipeline
			SDL_BindGPUGraphicsPipeline(render_pass, scn.pipelines.at(0).get());

			// Draw Indexed
			SDL_DrawGPUIndexedPrimitives(render_pass, scn.index_count, scn.instance_count, 0, 0, 0);

			// For Grid Plan -----------------------------------------------------------------------------------------------------------------------
			// Graphics Pipeline
			SDL_BindGPUGraphicsPipeline(render_pass, scn.pipelines.at(1).get());

			// Draw Indexed
			SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
		}
		SDL_EndGPURenderPass(render_pass);

		SDL_SubmitGPUCommandBuffer(cmd_buf);
	}
}

/*
 * SDL functions called every frame
 */
namespace frame
{

	void update_instance_buffer(const sdl3::context &ctx, const io::byte_span instances, sdl3::scene &rndr)
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

}