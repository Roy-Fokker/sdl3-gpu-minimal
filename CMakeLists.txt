cmake_minimum_required(VERSION 3.31)

#---------------------------------------------------------------------------------------
# set cmake specific configurations

# Enable scan for dependencies
set(CXX_SCAN_FOR_MODULES ON)
# disable C++ extensions
set(CMAKE_CXX_EXTENSIONS OFF)
# force C++ version to 23
set(CMAKE_CXX_STANDARD 23)
# standard is required
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the experimental flag
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
	"0e5b6991-d74f-4b3d-a41c-cf096e0b2508" # current value as per https://www.kitware.com/import-std-in-cmake-3-30/
)

# tell CMake we want to use 'import std' 
# will get enabled for all targets declared after this
set(CMAKE_CXX_MODULE_STD 1)

#---------------------------------------------------------------------------------------
# project and language to be used
project(sdl3-gpu-minimal
	LANGUAGES CXX
	DESCRIPTION "C++23 based Minimal SDL3 GPU Example.")

#---------------------------------------------------------------------------------------

#---------------------------------------------------------------------------------------
# set platform specific configurations

# Set Properties for Windows specific configurations
list(APPEND platform_definitions
	_CRT_SECURE_NO_WARNINGS     # Don't want MSVC's CRT warnings
	NOMINMAX                    # Don't want Window.h to define min and max macros
	WIN32_LEAN_AND_MEAN         # Windows.h should only include the basics
)

#---------------------------------------------------------------------------------------
# set application specific configurations

# name of this application/library
set(PRJ_APP_NAME "sdl3gpu-min-app")

# create executable for initial setup
add_executable(${PRJ_APP_NAME})

# ensure it's Standard C++ 23
target_compile_features(${PRJ_APP_NAME} PRIVATE cxx_std_23)

# set preprocessor defines
target_compile_definitions(${PRJ_APP_NAME}
	PRIVATE 
		UNICODE _UNICODE            # Tell compiler we are using UNICODE
		${platform_definitions}     # Get platform specific definitions
)

# source files for this application
target_sources(${PRJ_APP_NAME}
	# Non C++ module source files
	PRIVATE
		sdl3-min-src.cpp
)

# libraries used by this application
target_link_libraries(${PRJ_APP_NAME}
	PRIVATE
)

#---------------------------------------------------------------------------------------

