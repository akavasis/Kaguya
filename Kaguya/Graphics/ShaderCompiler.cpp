#include "pch.h"
#include "ShaderCompiler.h"

std::wstring ProfileString(Shader::Type Type, Shader::Model Model)
{
	std::wstring profileString;
	switch (Type)
	{
	case Shader::Type::Vertex: profileString = L"vs_"; break;
	case Shader::Type::Hull: profileString = L"hs_"; break;
	case Shader::Type::Domain: profileString = L"ds_"; break;
	case Shader::Type::Geometry: profileString = L"gs_"; break;
	case Shader::Type::Pixel: profileString = L"ps_"; break;
	case Shader::Type::Compute: profileString = L"cs_"; break;
	}

	switch (Model)
	{
	case Shader::Model::Model_6_3: profileString += L"6_3"; break;
	case Shader::Model::Model_6_4: profileString += L"6_4"; break;
	}
	return profileString;
}

ShaderCompiler::ShaderCompiler()
{
	ThrowCOMIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_DxcCompiler.ReleaseAndGetAddressOf())));
	ThrowCOMIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(m_DxcLibrary.ReleaseAndGetAddressOf())));
}

Shader ShaderCompiler::LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	std::filesystem::path filePath = std::filesystem::absolute(pPath);
	if (!std::filesystem::exists(filePath))
	{
		throw std::exception("File Not Found");
	}

	// https://developer.nvidia.com/dx12-dos-and-donts
	// Use the /all_resources_bound / D3DCOMPILE_ALL_RESOURCES_BOUND compile flag if possible This allows for the compiler to do a better job at optimizing texture accesses.We have seen frame rate improvements of > 1 % when toggling this flag on.
	LPCWSTR cmdlineArgs[] =
	{
		L"-all_resources_bound",
		L"-WX",				// Warnings as errors
#ifdef _DEBUG
		L"-Zi",				// Debug info
		L"-Qembed_debug",	// Embed debug info into the shader
		L"-Od",				// Disable optimization
#else
		L"-O3",				// Optimization level 3
#endif
	};

	Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSource;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> pDxcIncludeHandler;

	UINT32 codePage = CP_ACP;
	ThrowCOMIfFailed(m_DxcLibrary->CreateBlobFromFile(filePath.c_str(), &codePage, pSource.ReleaseAndGetAddressOf()));

	ThrowCOMIfFailed(m_DxcLibrary->CreateIncludeHandler(pDxcIncludeHandler.ReleaseAndGetAddressOf()));

	Microsoft::WRL::ComPtr<IDxcOperationResult> pDxcOperationResult;
	ThrowCOMIfFailed(m_DxcCompiler->Compile(
		pSource.Get(),
		filePath.c_str(),
		pEntryPoint,
		ProfileString(Type, Shader::Model::Model_6_3).data(),
		cmdlineArgs, ARRAYSIZE(cmdlineArgs),
		nullptr, 0,
		pDxcIncludeHandler.Get(),
		pDxcOperationResult.ReleaseAndGetAddressOf()));

	HRESULT hr;
	pDxcOperationResult->GetStatus(&hr);
	if (SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<IDxcBlob> pDxcBlob;
		pDxcOperationResult->GetResult(pDxcBlob.ReleaseAndGetAddressOf());
		return Shader(Type, pDxcBlob);
	}
	else
	{
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pError;
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pError16;

		pDxcOperationResult->GetErrorBuffer(pError.ReleaseAndGetAddressOf());
		m_DxcLibrary->GetBlobAsUtf16(pError.Get(), pError16.ReleaseAndGetAddressOf());
		OutputDebugString((LPCWSTR)pError16->GetBufferPointer());
		assert(false);
		return Shader(Shader::Type::Unknown, nullptr);
	}
}