#include "ShaderCompilerShared.h"

#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "ShaderCompilerDXC.h"

namespace ShaderCompilation
{
	bool CompileShader(
	    const ShaderCompilationInfo& compInfo,
	    const std::vector<std::string>& includeDirectories, // TODO: Remove this and decide it statically.
	    ShaderArtifact& outArtifact
	)
	{
		bool compSuccessful = false;

		switch (compInfo.compiler)
		{
			case ShaderCompiler::DXC:
				compSuccessful = ShaderCompilation::DXC::CompileShader(compInfo, includeDirectories, outArtifact);
				break;

			default:
				PRINT_ERROR("Unknown shader compiler type.");
				CHECK_UNREACHABLE();
		}

		return compSuccessful;
	}
} // namespace ShaderCompilation
