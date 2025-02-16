module;

// SDL 3 header
#include <SDL3/SDL.h>

// DDS and KTX texture file loader
// DDSKTX_IMPLEMENT must be defined in exactly one translation unit
// doesn't matter for this example, but it's good practice to define it in the same file as the implementation
#define DDSKTX_IMPLEMENT
#include <dds-ktx.h>

export module io;

import std;
import logs;

export namespace io
{
	// Convience alias for a span of bytes
	using byte_array = std::vector<std::byte>;
	using byte_span  = std::span<const std::byte>;
	using byte_spans = std::span<byte_span>;

	// Simple function to read a file in binary mode.
	auto read_file(const std::filesystem::path &filename) -> byte_array
	{
		msg::info(std::format("Reading file: {}", filename.string()));

		auto file = std::ifstream(filename, std::ios::in | std::ios::binary);

		msg::error(file.good(), "failed to open file!");

		auto file_size = std::filesystem::file_size(filename);
		auto buffer    = byte_array(file_size);

		file.read(reinterpret_cast<char *>(buffer.data()), file_size);

		file.close();

		return buffer;
	}

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
		byte_array data;
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
		auto sub_img_rng = vw::cartesian_product(layer_rng, mipmap_rng)                        // cartesian product will produce a pair-wise range
		                 | vw::transform([&](auto &&layer_mip_pair) -> image_data::sub_image { // transform will convert ddsktx_sub_data to image_data::sub_image
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