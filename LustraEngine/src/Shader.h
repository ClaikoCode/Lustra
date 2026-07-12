#pragma once

#include "LustraVulkan.h"
#include "Resource.h"

namespace Resource
{
	// TODO: Move this to its own file
	struct Shader : ResourceTag
	{
		vk::ShaderModule module;
		ShaderArtifact artifact;
	};

	void DestroyShader(Handle<Shader> shaderHandle);
} // namespace Resource
