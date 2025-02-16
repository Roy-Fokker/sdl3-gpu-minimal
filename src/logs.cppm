module;

// For assert macro
#include <cassert>

// SDL 3 header
#include <SDL3/SDL.h>

export module logs;

import std;

/*
 * ANSI Colored logging messages
 */
export namespace msg
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