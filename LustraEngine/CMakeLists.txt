# CMake project for LustraEngine

add_subdirectory(vendor)

add_executable(LustraEngine "src/main.cpp")

target_link_libraries(LustraEngine PRIVATE LustraLib)