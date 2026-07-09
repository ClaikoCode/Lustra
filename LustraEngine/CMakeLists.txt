# CMake project for LustraEngine

# At minimum use Vulkan SDK version 1.3
find_package(Vulkan 1.3 REQUIRED COMPONENTS dxc)

# Add vendored libraries
add_subdirectory(vendor)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_executable(LustraEngine ${ENGINE_SOURCES})

target_link_libraries(LustraEngine PRIVATE
	LustraLib
	Vulkan::Vulkan
	Vulkan::dxc_lib
	glm
	vma
	stb-image
	tinygltf
	SDL3::SDL3-static
	spirv-reflect
	ktx
)

# Copy executable files to Run folder.
add_custom_command(TARGET LustraEngine POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:LustraEngine> ${CMAKE_SOURCE_DIR}/Run/
)

target_compile_definitions(
	LustraEngine PRIVATE LUSTRA_CORE_ASSET_ROOT="Assets/"
)
