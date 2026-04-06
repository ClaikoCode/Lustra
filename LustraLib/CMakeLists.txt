# CMake project for LustraLib

add_library(LustraLib STATIC "src/Logger.cpp" "src/Utils.cpp")
target_include_directories(LustraLib 
	PUBLIC	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/LustraLib"
)