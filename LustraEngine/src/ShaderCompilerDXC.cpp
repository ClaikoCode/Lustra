#include "ShaderCompilerDXC.h"

#include "ArgBuilderDXC.h"
#include "LustraLib/Assert.h"
#include "LustraLib/Utils.h"
#include "dxc/WinAdapter.h"

static CComPtr<IDxcLibrary> sLibrary;
static CComPtr<IDxcCompiler3> sCompiler;
static CComPtr<IDxcUtils> sUtils;

// Temporary assert for HR values.
#define ASSERT_HR(hrExpression) ENSURE_EX(SUCCEEDED((hrExpression)), "HR Failed")

namespace
{
	void HandleDXCCompilationError(const CComPtr<IDxcBlobEncoding>& errorBlob)
	{
		std::string errorString;

		LPVOID bufferPtr        = errorBlob->GetBufferPointer();
		const SIZE_T bufferSize = errorBlob->GetBufferSize();

		// Check encoding.
		{
			UINT32 encodingCodePage;
			BOOL encodingKnown;
			errorBlob->GetEncoding(&encodingKnown, &encodingCodePage);

			ENSURE(encodingKnown == true);

			if (encodingCodePage == DXC_CP_UTF8)
			{
				const char* errorPtr = reinterpret_cast<const char*>(bufferPtr);
				errorString          = std::string(errorPtr, bufferSize);
			}
			else if (encodingCodePage == DXC_CP_UTF16)
			{
				const auto* errorPtr = reinterpret_cast<const wchar_t*>(bufferPtr);
				errorString          = Utils::ConvertStringWideToNarrow(std::wstring(errorPtr, bufferSize).c_str());
			}
		}

		if (!errorString.empty())
		{
			errorString.pop_back(); // Remove null terminator.
		}

		const std::string separator = "-----------------";
		const std::string header    = separator + " START OF COMPILATION ERROR " + separator;
		const std::string footer    = separator + " END OF COMPILATION ERROR " + separator;

		const std::string errorOut = header + "\n" + errorString + "\n" + footer;

		PRINT_ERROR("Shader compilation failed [DXC]:\n\n{}", errorOut);
	}
} // namespace

// Saves all the filenames of included files during shader compilation.
class DependencyTrackingIncludeHandler : public IDxcIncludeHandler
{
  public:
	DependencyTrackingIncludeHandler(IDxcUtils* pUtils)
	{
		pUtils->CreateDefaultIncludeHandler(&m_defaultIncludeHandler);
	}

	// IDxcIncludeHandler implementation
	HRESULT STDMETHODCALLTYPE
	LoadSource(_In_z_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
	{
		std::wstring filename = pFilename;

		// Remove the first three characters from filename (".\\")
		if (filename.size() > 2)
		{
			filename = filename.substr(2);
		}

		// Delegate to the default handler
		const HRESULT hr = m_defaultIncludeHandler->LoadSource(filename.c_str(), ppIncludeSource);

		if (SUCCEEDED(hr))
		{
			// Add to our tracking set if everything was ok.
			m_includedFiles.insert(Utils::ConvertStringWideToNarrow(filename.c_str()));
		}

		return hr;
	}

	std::unordered_set<std::string> ExtractIncludeFilepaths()
	{
		return std::move(m_includedFiles);
	}

	// IUnknown implementation
	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return 0;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		return 0;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
	{
		UNUSED_VAR(iid);
		UNUSED_VAR(ppvObject);

		return E_NOINTERFACE;
	}

  private:
	// The default include handler to delegate actual file loading
	CComPtr<IDxcIncludeHandler> m_defaultIncludeHandler;

	// Set of included files for the current compilation
	std::unordered_set<std::string> m_includedFiles;
};

namespace ShaderCompilation::DXC
{
	void Init()
	{
		ENSURE(sLibrary == nullptr);
		ENSURE(sCompiler == nullptr);
		ENSURE(sUtils == nullptr);

		ASSERT_HR(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&sLibrary)));
		ASSERT_HR(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&sCompiler)));
		ASSERT_HR(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&sUtils)));
	}

	bool CompileShader(
	    const ShaderCompilationInfo& compInfo,
	    const std::vector<std::string>& includeDirectories,
	    ShaderArtifact& outArtifact
	)
	{
		const std::string filePath   = compInfo.shaderPath.string();
		const std::wstring filePathW = Utils::ConvertStringNarrowToWide(filePath.c_str());

		PRINT_DEBUG("Compiling shader: '{}'", filePath);

		DxcBuffer dxcBuffer = {};
		CComPtr<IDxcBlobEncoding> blobEncoding;
		{
			ASSERT_HR(sLibrary->CreateBlobFromFile(filePathW.c_str(), nullptr, &blobEncoding));

			BOOL known;
			UINT32 codePage;
			ASSERT_HR(blobEncoding->GetEncoding(&known, &codePage));

			dxcBuffer.Encoding = codePage;
			dxcBuffer.Ptr      = blobEncoding->GetBufferPointer();
			dxcBuffer.Size     = blobEncoding->GetBufferSize();
		}

		ShaderCompilationArgsBuilder argBuilder;
		{
			argBuilder.AddArgFilename(filePathW);
			argBuilder.AddArgSPIRV();

			argBuilder.AddArgEntryPoint(Utils::ConvertStringNarrowToWide(compInfo.entryPoint.data()));
			argBuilder.AddArgShaderModel(compInfo.shaderModel, compInfo.shaderType);

			for (const auto& includeDir : includeDirectories)
			{
				argBuilder.AddArgIncludeDirectory(Utils::ConvertStringNarrowToWide(includeDir.c_str()));
			}

#ifdef _DEBUG
			argBuilder.AddArgDebugInfo();
			argBuilder.AddArgEmbedDebug();
			argBuilder.AddArgOptimizationLevel(0);
#else
			argBuilder.AddArgOptimizationLevel(3);
#endif

			for (const auto& defineString : compInfo.defines)
			{
				argBuilder.AddArgDefine(Utils::ConvertStringNarrowToWide(defineString.c_str()));
			}
		}

		DependencyTrackingIncludeHandler includeHandler(sUtils);
		CComPtr<IDxcOperationResult> compResult;
		{
			const std::vector<std::wstring>& args = argBuilder.GetArgs();

			std::vector<LPCWSTR> argPtrs;
			argPtrs.reserve(args.size());
			for (const auto& arg : args)
			{
				argPtrs.push_back(arg.c_str());
			}

			ASSERT_HR(sCompiler->Compile(
			    &dxcBuffer,
			    argPtrs.data(),
			    static_cast<UINT32>(argPtrs.size()),
			    &includeHandler,
			    IID_PPV_ARGS(&compResult)
			));

			HRESULT compStatus;
			ASSERT_HR(compResult->GetStatus(&compStatus));
			if (FAILED(compStatus))
			{
				CComPtr<IDxcBlobEncoding> errorBlob = nullptr;
				ASSERT_HR(compResult->GetErrorBuffer(&errorBlob));

				if (errorBlob != nullptr)
				{
					::HandleDXCCompilationError(errorBlob);
				}

				return false;
			}
		}

		CComPtr<IDxcBlob> code;
		ASSERT_HR(compResult->GetResult(&code));

		ENSURE(code != nullptr);

		// Copy the shader data.
		outArtifact.spirvData.resize(code->GetBufferSize());
		std::memcpy(outArtifact.spirvData.data(), code->GetBufferPointer(), code->GetBufferSize());

		outArtifact.includeFiles = includeHandler.ExtractIncludeFilepaths();

		PRINT_DEBUG("Compilation successful.");

		return true;
	}
} // namespace ShaderCompilation::DXC
