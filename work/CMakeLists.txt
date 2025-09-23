################################################################
#                          Created by:                         #
#                    Ben Allen & Joshua Scott                  #
################################################################

# require new behaviour of: CMP0054
cmake_minimum_required(VERSION 3.1)

# Include Extra CMake capabilities
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(CGRAFunctions)

# Project Name
set(CGRA_PROJECT "base" CACHE STRING "CGRA Project Name")

# Project
project("CGRA_PROJECT_${CGRA_PROJECT}" CXX C)

# Enable IDE Project Folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT base)


#########################################################
# Force Output Directories
# In general, this isn't a very friendly thing to do, but
# we'll do it anyway so the exes are in a reliable place.
#########################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")



#########################################################
# Find OpenGL
#########################################################

find_package(OpenGL REQUIRED)



#########################################################
# Find OpenMP
#########################################################

find_package(OpenMP)
if (OPENMP_FOUND)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
	add_definitions(-DCGRA_HAVE_OPENMP)
endif()



#########################################################
# Include Subprojects
#########################################################

add_subdirectory("${PROJECT_SOURCE_DIR}/ext/glfw")
include_directories("${PROJECT_SOURCE_DIR}/ext/glfw/include")
add_subdirectory("${PROJECT_SOURCE_DIR}/ext/glew-1.10.0")
add_subdirectory("${PROJECT_SOURCE_DIR}/ext/stb")
add_subdirectory("${PROJECT_SOURCE_DIR}/ext/imgui")
add_subdirectory("${PROJECT_SOURCE_DIR}/ext/glm")
include_directories("${PROJECT_SOURCE_DIR}/ext") # Add ext in order to access glm subfiles (hack)
include_directories("${PROJECT_SOURCE_DIR}/src") # Add source to include directory



#########################################################
# Set Compiler Flags
#########################################################

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	# # C++ latest
	add_compile_options(/std:c++latest)
	# UTF-8 source and execution charsets
	add_compile_options(/utf-8)
	# Full normal warnings, multithreaded build
	add_compile_options(/W4 /MP)
	# Disable C4800: forcing X to bool (performance warning)
	add_compile_options(/wd4800)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-O2>")
	# # C++17, full normal warnings
	add_compile_options(-std=c++1z -Wall -Wextra -pedantic)
	# C++14, full normal warnings
	# add_compile_options(-std=c++1y -Wall -Wextra -pedantic)
	# don't export by default
	add_compile_options(-fvisibility=hidden)
	# Threading support, enable SSE2
	add_compile_options(-pthread -msse2)
	# Promote missing return to error
	add_compile_options(-Werror=return-type)
	# enable coloured output if gcc >= 4.9
	execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
	if (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
		add_compile_options(-fdiagnostics-color)
	endif()
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(Apple)?Clang$")
	add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-O2>")
	# # C++17, full normal warnings
	add_compile_options(-std=c++1z -Wall -Wextra -pedantic)
	# C++14, full normal warnings
	# add_compile_options(-std=c++1y -Wall -Wextra -pedantic)
	# don't export by default
	add_compile_options(-fvisibility=hidden)
	# Threading support, enable SSE2
	add_compile_options(-pthread -msse2)
	# Promote missing return to error
	add_compile_options(-Werror=return-type)
endif()



#########################################################
# Source Files
#########################################################

add_subdirectory(src) # Primary source files
add_subdirectory(res) # Resources like shaders (show up in IDE)
set_property(TARGET ${CGRA_PROJECT} PROPERTY FOLDER "CGRA")
