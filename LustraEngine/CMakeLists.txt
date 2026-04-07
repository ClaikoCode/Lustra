# CMake project for LustraEngine

# Check that the SDK is actually installed (not just driver-provided loader)
if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR
        "VULKAN_SDK environment variable not set.\n"
        "The Vulkan SDK is required for building Lustra, which can be downloaded from: https://vulkan.lunarg.com/sdk/home\n"
        "Make sure to restart your IDE/terminal after installation!"
    )
endif()

# At minimum use Vulkan SDK version 1.3 
find_package(Vulkan 1.3 REQUIRED COMPONENTS dxc)

# Add vendored libraries
add_subdirectory(vendor)

add_executable(LustraEngine "src/main.cpp" "src/Window.h" "src/Window.cpp" "src/App.h" "src/App.cpp" "src/Graphics.h" "src/Graphics.cpp" "src/SDLAssert.h")

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