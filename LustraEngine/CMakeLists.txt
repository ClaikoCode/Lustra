# CMake project for LustraEngine

# At minimum use Vulkan SDK version 1.3
find_package(Vulkan 1.3 REQUIRED COMPONENTS dxc)

# Add vendored libraries
add_subdirectory(vendor)

add_executable(LustraEngine "src/main.cpp" "src/Window.h" "src/Window.cpp" "src/App.h" "src/App.cpp" "src/Graphics.h" "src/Graphics.cpp" "src/SDLAssert.h" "src/GraphicsUtils.h" "src/GraphicsUtils.cpp")

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

