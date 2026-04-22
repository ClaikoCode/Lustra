# CMake project file for LustraEngine/vendor
# For each vendored library, ther exists a VERSION.txt file that contains information about what release version
# was downloaded and when it was downloaded.

# ==========================================
#           HEADER ONLY LIBRARIES
# ==========================================

add_library(glm INTERFACE)
target_include_directories(glm SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/glm")

add_library(vma INTERFACE)
target_include_directories(vma SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vma")

add_library(stb-image INTERFACE)
target_include_directories(stb-image SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb")

add_library(tinygltf INTERFACE)
target_include_directories(tinygltf SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/tinygltf")

# ==========================================
#          SOURCE FILE LIBRARIES
# ==========================================

# Disable warnings for third-party code only for vendor library cmake directories.
if(MSVC)
    add_compile_options(/W0)
else()
    add_compile_options(-w)
endif()

set(SDL_SHARED          OFF CACHE BOOL "" FORCE)
set(SDL_STATIC          ON  CACHE BOOL "" FORCE)
set(SDL_TEST            OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES        OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL         OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_TESTS   OFF CACHE BOOL "" FORCE)
add_subdirectory(sdl3 EXCLUDE_FROM_ALL)

set(BUILD_SHARED_LIBS           OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS           OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS           OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC             OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS   OFF CACHE BOOL "" FORCE)
add_subdirectory(ktx EXCLUDE_FROM_ALL)
target_compile_definitions(ktx INTERFACE KHRONOS_STATIC)
set_target_properties(ktx PROPERTIES SYSTEM TRUE)

add_library(spirv-reflect "${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect/spirv-reflect/spirv_reflect.cpp")
target_include_directories(spirv-reflect SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect")
