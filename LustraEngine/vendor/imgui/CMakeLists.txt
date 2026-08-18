# CMake project file for LustraEngine/vendor/imgui

add_library(imgui STATIC
	"imgui.cpp"
	"imgui_draw.cpp"
	"imgui_tables.cpp"
	"imgui_widgets.cpp"
	"imgui_demo.cpp"
	"imgui_impl_vulkan.cpp"
	"imgui_impl_sdl3.cpp"
)

target_include_directories(imgui SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL3::SDL3-static)

