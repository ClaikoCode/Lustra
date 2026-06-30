# CMake project for LustraEngine

# At minimum use Vulkan SDK version 1.3
find_package(Vulkan 1.3 REQUIRED COMPONENTS dxc)

# Add vendored libraries
add_subdirectory(vendor)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

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

