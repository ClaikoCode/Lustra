# Root CMake project file for Lustra

# 3.30 is minimum support for C++23 and C++26, which this project might use.
cmake_minimum_required(VERSION 3.30...3.31)

# Disables CMake automatically adding /W3 to MSVC default compiler flags.
if(POLICY CMP0092)
	cmake_policy(SET CMP0092 NEW)
endif()

# Tell CMake to use the old behavior for this policy
# This prevents it from trying to set MSVC-specific debug format
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 OLD)
endif()

project(Lustra DESCRIPTION "Lustra Engine + Lib" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_definitions()

# Global compile options.
if(MSVC)
	message(STATUS "Using MSVC compile flags")
	add_compile_options(/W4 /WX)
	add_compile_options(/Zc:preprocessor) # Preprocessor conformance mode (fixes __VA_OPT__ support).
else()
	message(STATUS "Using non-MSVC compile flags")
	add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_subdirectory("LustraLib")
add_subdirectory("LustraEngine") # Depends on LustraLib