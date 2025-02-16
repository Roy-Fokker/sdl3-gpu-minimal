// For assert macro
#include <cassert>

// SDL 3 header
#include <SDL3/SDL.h>

// GLM configuration and headers
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // GLM clip space should be in Z-axis to 0 to 1
#define GLM_FORCE_LEFT_HANDED       // GLM should use left-handed coordinates, +z goes into screen
#define GLM_FORCE_RADIANS           // GLM should always use radians not degrees.
#include <glm/glm.hpp>              // Required for glm::vec3/4/mat4/etc
#include <glm/ext.hpp>              // Required for glm::perspective function

// Standard Library module
import std;

import logs;
import io;
import sdl3_init;
import sdl3_scene;

// literal suffixes for strings, string_view, etc
using namespace std::literals;

/*
 * To manage application specific state data
 */
namespace app
{
	auto quit = false;

	void update(float &angle)
	{
		auto *key_states = SDL_GetKeyboardState(nullptr);

		if (key_states[SDL_SCANCODE_ESCAPE])
			quit = true;

		if (key_states[SDL_SCANCODE_A] or key_states[SDL_SCANCODE_LEFT])
			angle -= 0.5f;
		if (key_states[SDL_SCANCODE_D] or key_states[SDL_SCANCODE_RIGHT])
			angle += 0.5f;

		if (angle >= 360.0f or angle <= -360.0f)
			angle = 0.0f;
	}

	struct vertex
	{
		glm::vec3 pos;
		glm::vec2 uv;
	};

	struct mesh
	{
		std::vector<vertex> vertices;
		std::vector<uint32_t> indices;
	};

	auto make_cube() -> mesh
	{
		auto x = 0.5f, y = 0.5f, z = 0.5f;

		auto vertices = std::vector<vertex>{
			// +X face
			{ { +x, -y, -z }, { 0.f, 1.f } },
			{ { +x, -y, +z }, { 0.f, 0.f } },
			{ { +x, +y, +z }, { 1.f, 0.f } },
			{ { +x, +y, -z }, { 1.f, 1.f } },
			// -X face
			{ { -x, -y, -z }, { 0.f, 1.f } },
			{ { -x, +y, -z }, { 1.f, 1.f } },
			{ { -x, +y, +z }, { 1.f, 0.f } },
			{ { -x, -y, +z }, { 0.f, 0.f } },
			// +Y face
			{ { -x, +y, -z }, { 0.f, 1.f } },
			{ { +x, +y, -z }, { 1.f, 1.f } },
			{ { +x, +y, +z }, { 1.f, 0.f } },
			{ { -x, +y, +z }, { 0.f, 0.f } },
			// -Y face
			{ { -x, -y, -z }, { 0.f, 1.f } },
			{ { -x, -y, +z }, { 0.f, 0.f } },
			{ { +x, -y, +z }, { 1.f, 0.f } },
			{ { +x, -y, -z }, { 1.f, 1.f } },
			// +Z face
			{ { -x, -y, +z }, { 0.f, 1.f } },
			{ { -x, +y, +z }, { 0.f, 0.f } },
			{ { +x, +y, +z }, { 1.f, 0.f } },
			{ { +x, -y, +z }, { 1.f, 1.f } },
			// -Z face
			{ { -x, -y, -z }, { 0.f, 1.f } },
			{ { +x, -y, -z }, { 1.f, 1.f } },
			{ { +x, +y, -z }, { 1.f, 0.f } },
			{ { -x, +y, -z }, { 0.f, 0.f } },
		};

		auto indices = std::vector<uint32_t>{
			0, 1, 2, 2, 3, 0,       // +X face
			4, 5, 6, 6, 7, 4,       // -X face
			8, 9, 10, 10, 11, 8,    // +Y face
			12, 13, 14, 14, 15, 12, // -Y face
			16, 17, 18, 18, 19, 16, // +Z face
			20, 21, 22, 22, 23, 20, // -Z face
		};

		return {
			vertices,
			indices,
		};
	}

	struct instance_data
	{
		std::vector<glm::mat4> transforms;
	};

	auto make_cube_instances() -> instance_data
	{
		auto cube_1 = glm::translate(glm::mat4(1.0f), glm::vec3{ 0.f, 0.f, 0.f });

		auto cube_2 = glm::translate(glm::mat4(1.0f), glm::vec3{ 0.f, 0.f, -3.f });

		auto cube_3 = glm::translate(glm::mat4(1.0f), glm::vec3{ 0.f, 0.f, 3.f });

		return {
			{
			  cube_1,
			  cube_2,
			  cube_3,
			},
		};
	}

	auto get_pipeline_desc() -> std::vector<sdl3::pipeline_desc>
	{
		using VA                                = SDL_GPUVertexAttribute;
		constexpr static auto VERTEX_ATTRIBUTES = std::array{
			VA{
			  .location    = 0,
			  .buffer_slot = 0,
			  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
			  .offset      = 0,
			},
			VA{
			  .location    = 1,
			  .buffer_slot = 0,
			  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
			  .offset      = sizeof(glm::vec3),
			},
			VA{
			  .location    = 2,
			  .buffer_slot = 1,
			  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
			  .offset      = 0,
			},
			VA{
			  .location    = 3,
			  .buffer_slot = 1,
			  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
			  .offset      = sizeof(glm::vec4),
			},
			VA{
			  .location    = 4,
			  .buffer_slot = 1,
			  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
			  .offset      = sizeof(glm::vec4) * 2,
			},
			VA{
			  .location    = 5,
			  .buffer_slot = 1,
			  .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
			  .offset      = sizeof(glm::vec4) * 3,
			},
		};

		using VBD                                 = SDL_GPUVertexBufferDescription;
		constexpr static auto VERTEX_BUFFER_DESCS = std::array{
			VBD{
			  .slot       = 0,
			  .pitch      = sizeof(vertex),
			  .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
			},
			VBD{
			  .slot               = 1,
			  .pitch              = sizeof(glm::mat4),
			  .input_rate         = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
			  .instance_step_rate = 1,
			},
		};

		auto vs_bin = io::read_file("shaders/instanced_mesh.vs_6_4.cso");
		auto fs_bin = io::read_file("shaders/textured_quad.ps_6_4.cso");

		auto grid_vs_bin = io::read_file("shaders/grid.vs_6_4.cso");
		auto grid_fs_bin = io::read_file("shaders/grid.ps_6_4.cso");

		return {
			{
			  .vertex = sdl3::shader_desc{
				.shader_binary        = vs_bin,
				.stage                = SDL_GPU_SHADERSTAGE_VERTEX,
				.uniform_buffer_count = 1,
			  },
			  .fragment = sdl3::shader_desc{
				.shader_binary = fs_bin,
				.stage         = SDL_GPU_SHADERSTAGE_FRAGMENT,
				.sampler_count = 1,
			  },
			  .vertex_attributes          = VERTEX_ATTRIBUTES,
			  .vertex_buffer_descriptions = VERTEX_BUFFER_DESCS,
			  .depth_test                 = true,
			},
			{
			  .vertex = sdl3::shader_desc{
				.shader_binary        = grid_vs_bin,
				.stage                = SDL_GPU_SHADERSTAGE_VERTEX,
				.uniform_buffer_count = 1,
			  },
			  .fragment = sdl3::shader_desc{
				.shader_binary = grid_fs_bin,
				.stage         = SDL_GPU_SHADERSTAGE_FRAGMENT,
				.sampler_count = 1,
			  },
			  .depth_test = true,
			  .cull_mode  = sdl3::cull_mode_t::none,
			},
		};
	}

	auto load_texture() -> io::image_data
	{
		return io::read_image_file("data/uv_grid.dds");
	}

	auto get_projection(uint32_t width, uint32_t height, float angle) -> std::array<glm::mat4, 2>
	{
		auto fov          = glm::radians(90.0f);
		auto aspect_ratio = static_cast<float>(width) / height;

		auto x = std::cosf(angle);
		auto y = 0; // 1.5f;
		auto z = std::sinf(angle);

		x = x * 2.5f;
		z = z * 2.5f;

		auto projection = glm::perspective(fov, aspect_ratio, 0.1f, 100.f);
		auto view       = glm::lookAt(glm::vec3(x, y, z),
		                              glm::vec3(0.f, 0.f, 0.f),
		                              glm::vec3(0.f, 1.f, 0.f));

		return {
			projection,
			view,
		};
	}
}

auto main() -> int
{
	constexpr auto app_title = "SDL3 GPU minimal example."sv;
	constexpr auto width     = 1920;
	constexpr auto height    = 1080;

	auto angle = 0.f;

	auto view_proj      = app::get_projection(width, height, glm::radians(angle));
	auto texture        = app::load_texture();
	auto cube_mesh      = app::make_cube();
	auto cube_instances = app::make_cube_instances();
	auto pl_descs       = app::get_pipeline_desc();

	auto ctx = sdl3::init_context(width, height, app_title);
	auto scn = sdl3::init_scene(
		ctx,
		pl_descs,
		io::as_byte_span(cube_mesh.vertices), static_cast<uint32_t>(cube_mesh.vertices.size()),
		io::as_byte_span(cube_mesh.indices), static_cast<uint32_t>(cube_mesh.indices.size()),
		io::as_byte_span(cube_instances.transforms), static_cast<uint32_t>(cube_instances.transforms.size()),
		texture);

	scn.clear_color = { 0.4f, 0.4f, 0.4f, 1.0f };

	auto e = SDL_Event{};
	while (not app::quit)
	{
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_EVENT_QUIT)
			{
				app::quit = true;
			}
		}
		sdl3::draw(ctx, scn, io::as_byte_span(view_proj));
		app::update(angle);

		view_proj = app::get_projection(width, height, glm::radians(angle));
	}

	sdl3::destroy_scene(scn);

	sdl3::destroy_context(ctx);

	return 0;
}