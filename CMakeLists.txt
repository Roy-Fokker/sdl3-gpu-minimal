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
# set platform specific configurations

# Set Properties for Windows specific configurations
list(APPEND platform_definitions
	_CRT_SECURE_NO_WARNINGS     # Don't want MSVC's CRT warnings
	NOMINMAX                    # Don't want Window.h to define min and max macros
	WIN32_LEAN_AND_MEAN         # Windows.h should only include the basics
)

#---------------------------------------------------------------------------------------
# Find shader compiler

find_program(DXC dxc DOC "DirectX 12 Shader Compiler")
if ("${DXC}" STREQUAL "DXC-NOTFOUND")
	message(FATAL_ERROR "[Error]: DirectX Shader Compiler not found")
endif()
message(STATUS "[Info]: Found DirectX Shader Compiler - ${DXC}")

#---------------------------------------------------------------------------------------
# Function to take shader file and compile it as dependency of program

function(target_hlsl_sources TARGET)
	# figure out how many files we have to configure given the pattern
	list(LENGTH ARGN count_HLSL)
	math(EXPR count_HLSL "${count_HLSL} / 3")

	# List of compiled shader output
	set(shader_files "")
	set(shader_sources "")

	# Are we in debug mode?
	string(TOLOWER ${CMAKE_BUILD_TYPE} compile_mode)
	if (${compile_mode} STREQUAL "debug")
		list(APPEND shader_pdb_options /Zi /Fd ${CMAKE_PDB_OUTPUT_DIRECTORY}/)
	endif()

	# Loop through all the pairs for filename:profile provided
	foreach(i RANGE 1 ${count_HLSL})
		math(EXPR fni "(${i}-1)*3")              # filename index
		math(EXPR pfi "${fni}+2")                # profile index
		list(GET ARGN ${fni} hlsl_filename)      # get the filename[i]
		list(GET ARGN ${pfi} hlsl_profile)       # get the profile[i]

		# get the absolute path of current source file
		file(REAL_PATH ${hlsl_filename} source_abs)

		if(NOT EXISTS ${source_abs})
			message(FATAL_ERROR "Cannot find shader file: ${source_abs}")
		endif()

		# get only the filename from absolute path
		cmake_path(GET source_abs STEM basename)
		set(basename "${basename}.${hlsl_profile}")

		# get only the parent directory of the file from absolute path
		cmake_path(GET source_abs PARENT_PATH source_fldr)
		get_filename_component(source_fldr "${source_fldr}" NAME)
		
		# shader output folder will be a subfolder in the binary directory
		set(shader_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)

		# full path to compiled output 
		set(output ${shader_dir}/${basename}.cso)

		# call vulkan sdk's dxc compiler with source and output arguments.
		add_custom_command(
			OUTPUT ${output}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_dir}
			COMMAND ${DXC} -E main -Fo ${output} -T ${hlsl_profile} ${source_abs} ${shader_pdb_options}
			DEPENDS ${source_abs}
			COMMENT "DXC Compiling SPIRV: ${hlsl_filename} -> ${output}"
			VERBATIM
		)

		list(APPEND shader_sources "${hlsl_filename}")
		list(APPEND shader_files "${output}")
	endforeach()

	# make a new variable to hold all output target names
	set(shader_group "${TARGET}_HLSL")
	# add custom target using new variable bound to output file of glslc step
	add_custom_target("${shader_group}"
					  DEPENDS "${shader_files}"
					  SOURCES "${shader_sources}"
	)

	# add compilation of this shader as dependency of the target
	add_dependencies("${TARGET}" "${shader_group}")
endfunction()

#---------------------------------------------------------------------------------------
# Function to copy data/asset files and mark them as dependency of program
function(target_data_assets TARGET)
	# get count of asset files
	list(LENGTH ARGN count_files)

	# output directory
	set(data_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data)

	# loop over all the files
	foreach(file_name IN ITEMS ${ARGN})
		# get absolute path of file
		file(REAL_PATH ${file_name} source_abs)

		# copied file path
		set(output_file ${data_dir}/${file_name})

		# call copy command
		add_custom_command(
			OUTPUT ${output_file}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${data_dir}
			COMMAND ${CMAKE_COMMAND} -E copy ${source_abs} ${data_dir}
			DEPENDS ${source_abs}
			COMMENT "Copy ${file_name} to ${data_dir}"
		)

		list(APPEND data_sources ${file_name})
		list(APPEND data_outputs ${output_file})
	endforeach()

	# add custom target for all the copy operations
	add_custom_target(${TARGET}_DATA
		DEPENDS ${data_outputs}
		SOURCES ${data_sources}
	)

	# add custom target as a dependency to TARGET
	add_dependencies("${TARGET}" ${TARGET}_DATA)
endfunction()


#---------------------------------------------------------------------------------------
# Look for libraries we want to use for this project
find_package(SDL3 CONFIG REQUIRED)  # SDL3 library 
find_package(Dds-ktx REQUIRED)      # DDS & KTS image file loader
find_package(glm CONFIG REQUIRED)   # Math Library

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
		src/main.cpp
	# C++ module source files
	PRIVATE FILE_SET app_modules TYPE CXX_MODULES FILES
		src/logs.cppm
		src/io.cppm
		src/sdl3-init.cppm
		src/sdl3-scene.cppm
)

# libraries used by this application
target_link_libraries(${PRJ_APP_NAME}
	PRIVATE
		SDL3::SDL3 # SDL library version 3.2
		glm::glm   # Math Library
)

# shader source files used by this application
target_hlsl_sources(${PRJ_APP_NAME}
	shaders/raw_triangle.vs.hlsl : vs_6_4
	shaders/raw_triangle.fs.hlsl : ps_6_4
	shaders/vertex_buffer_triangle.vs.hlsl : vs_6_4
	shaders/instanced_shapes.vs.hlsl : vs_6_4
	shaders/textured_quad.vs.hlsl : vs_6_4
	shaders/textured_quad.fs.hlsl : ps_6_4
	shaders/textured_mesh.vs.hlsl : vs_6_4
	shaders/instanced_mesh.vs.hlsl : vs_6_4
)

# Data files/Assets used by this application
target_data_assets(${PRJ_APP_NAME}
	textures/uv_grid.dds
)

#---------------------------------------------------------------------------------------
