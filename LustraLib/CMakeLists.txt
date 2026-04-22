# CMake project for LustraLib

add_library(LustraLib STATIC "src/Logger.cpp" "src/Utils.cpp")
target_include_directories(LustraLib 
	PUBLIC	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/LustraLib"
)

if(WIN32)
	# Makes MSVC targets give warnings about '_s' like functions being deprecated.
    target_compile_definitions(LustraLib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()