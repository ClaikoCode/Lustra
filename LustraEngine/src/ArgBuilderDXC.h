#pragma once
#include "LustraLib/Assert.h"
#include "ShaderCompilerShared.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

class ShaderCompilationArgsBuilder
{
  public:
	void AppendCompilationToken(const std::wstring& token)
	{
		mCompArgs.push_back(token);
	}

	void AddArgFilename(const std::wstring& filename)
	{
		AppendCompilationToken(filename);
	}

	void AddArgShaderModel(ShaderModel shaderModel, ShaderType shaderType)
	{
		AppendCompilationToken(L"-T");
		AppendCompilationToken(ShaderModelArg(shaderModel, shaderType));
	}

	void AddArgEntryPoint(const std::wstring& entryPoint)
	{
		AppendCompilationToken(L"-E");
		AppendCompilationToken(entryPoint);
	}

	void AddArgDebugInfo()
	{
		AppendCompilationToken(L"-Zi");
	}

	void AddArgEmbedDebug()
	{
		AppendCompilationToken(L"-Qembed_debug");
	}

	void AddArgOptimizationLevel(uint16_t level)
	{
		level = std::min<uint16_t>(level, 3); // No optimzation options available over level 3.
		AppendCompilationToken(std::wstring(L"-O") + std::to_wstring(level));
	}

	void AddArgIncludeDirectory(const std::wstring& includeDir)
	{
		AppendCompilationToken(L"-I");
		AppendCompilationToken(includeDir);
	}

	void AddArgDefine(const std::wstring& define)
	{
		AppendCompilationToken(L"-D");
		AppendCompilationToken(define);
	}

	void AddArgSPIRV()
	{
		AppendCompilationToken(L"-spirv");
	}

	const std::vector<std::wstring>& GetArgs() const
	{
		return mCompArgs;
	}

  private:
	static std::wstring ShaderModelArg(ShaderModel shaderModel, ShaderType shaderType)
	{
		static const std::unordered_map<ShaderModel, std::wstring> sShaderModelArg = {
		    {ShaderModel6_1, L"6_1"},
		    {ShaderModel6_3, L"6_3"},
		};
		ENSURE(shaderModel != ShaderModelUnknown && shaderType != ShaderTypeUnknown);
		const std::wstring& shaderModelStr = sShaderModelArg.at(shaderModel);
		std::wstring shaderTypeStr;
		switch (shaderType)
		{
			case ShaderTypeVS:
				shaderTypeStr += L"vs";
				break;
			case ShaderTypeHS:
				shaderTypeStr += L"hs";
				break;
			case ShaderTypeDS:
				shaderTypeStr += L"ds";
				break;
			case ShaderTypeGS:
				shaderTypeStr += L"gs";
				break;
			case ShaderTypeFS:
				shaderTypeStr += L"ps";
				break;
			case ShaderTypeCS:
				shaderTypeStr += L"cs";
				break;
			default:
				CHECK_UNREACHABLE();
				break;
		}
		return shaderTypeStr + L"_" + shaderModelStr;
	}

	std::vector<std::wstring> mCompArgs;
};
