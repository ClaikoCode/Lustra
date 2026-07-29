#include "App.h"
#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "LustraLib/Utils.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#define STBI_ASSERT(x) VALIDATE_EX(x, "STBI Assertion hit.")
#include "stb/stb_image.h"

#define TINYGLTF3_IMPLEMENTATION
// Asserts are not used in tinygltf v3 yet but might be in the future.
#define TINYGLTF3_ASSERT(x) LUSTRA_ASSERT(x)
#include "tinygltf/tiny_gltf_v3.h"

int main(int argc, char* argv[])
{
	UNUSED_VAR(argc);
	UNUSED_VAR(argv);

	App app = App("Lustra App");
	app.CreateWindow("Lustra (Vulkan)", 1280, 720);

	if (app.RunApp())
	{
		PRINT_ERROR("App exited with errors.");
	}

	return 0;
}
