#include "pch.h"
#include "ShaderCompiler.h"

using Microsoft::WRL::ComPtr;

std::wstring ShaderProfileString(Shader::Type Type, ShaderCompiler::Profile Profile)
{
	std::wstring profileString;
	switch (Type)
	{
	case Shader::Type::Vertex:					profileString = L"vs_"; break;
	case Shader::Type::Hull:					profileString = L"hs_"; break;
	case Shader::Type::Domain:					profileString = L"ds_"; break;
	case Shader::Type::Geometry:				profileString = L"gs_"; break;
	case Shader::Type::Pixel:					profileString = L"ps_"; break;
	case Shader::Type::Compute:					profileString = L"cs_"; break;
	case Shader::Type::Mesh:					profileString = L"ms_"; break;
	}

	switch (Profile)
	{
	case ShaderCompiler::Profile::Profile_6_3:	profileString += L"6_3"; break;
	case ShaderCompiler::Profile::Profile_6_4:	profileString += L"6_4"; break;
	case ShaderCompiler::Profile::Profile_6_5:	profileString += L"6_5"; break;
	}

	return profileString;
}

std::wstring LibraryProfileString(ShaderCompiler::Profile Profile)
{
	std::wstring profileString = L"lib_";
	switch (Profile)
	{
	case ShaderCompiler::Profile::Profile_6_3:	profileString += L"6_3"; break;
	case ShaderCompiler::Profile::Profile_6_4:	profileString += L"6_4"; break;
	case ShaderCompiler::Profile::Profile_6_5:	profileString += L"6_5"; break;
	}

	return profileString;
}

ShaderCompiler::ShaderCompiler()
{
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_DxcCompiler.ReleaseAndGetAddressOf())));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(m_DxcLibrary.ReleaseAndGetAddressOf())));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_DxcUtils.ReleaseAndGetAddressOf())));
}

void ShaderCompiler::SetIncludeDirectory(const std::filesystem::path& pPath)
{
	m_IncludeDirectory = pPath;
}

Shader ShaderCompiler::CompileShader(Shader::Type Type, const std::filesystem::path& Path, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines) const
{
	auto ProfileString	= ShaderProfileString(Type, ShaderCompiler::Profile::Profile_6_5);
	auto pDxcBlob		= Compile(Path, pEntryPoint, ProfileString.data(), ShaderDefines);

	DxcBuffer DxcBuffer = {};
	DxcBuffer.Ptr		= pDxcBlob->GetBufferPointer();
	DxcBuffer.Size		= pDxcBlob->GetBufferSize();
	DxcBuffer.Encoding	= CP_ACP;
	ComPtr<ID3D12ShaderReflection> pShaderReflection;
	ThrowIfFailed(m_DxcUtils->CreateReflection(&DxcBuffer, IID_PPV_ARGS(pShaderReflection.ReleaseAndGetAddressOf())));

	return Shader(Type, pDxcBlob, pShaderReflection);
}

Library ShaderCompiler::CompileLibrary(const std::filesystem::path& Path) const
{
	auto ProfileString	= LibraryProfileString(ShaderCompiler::Profile::Profile_6_5);
	auto pDxcBlob		= Compile(Path, L"", ProfileString.data(), {});

	DxcBuffer DxcBuffer = {};
	DxcBuffer.Ptr		= pDxcBlob->GetBufferPointer();
	DxcBuffer.Size		= pDxcBlob->GetBufferSize();
	DxcBuffer.Encoding	= CP_ACP;
	ComPtr<ID3D12LibraryReflection> pLibraryReflection;
	ThrowIfFailed(m_DxcUtils->CreateReflection(&DxcBuffer, IID_PPV_ARGS(pLibraryReflection.ReleaseAndGetAddressOf())));

	return Library(pDxcBlob, pLibraryReflection);
}

ComPtr<IDxcBlob> ShaderCompiler::Compile(const std::filesystem::path& Path, LPCWSTR pEntryPoint, LPCWSTR pProfile, const std::vector<DxcDefine>& ShaderDefines) const
{
	assert(std::filesystem::exists(Path) && "File Not Found");

	// https://developer.nvidia.com/dx12-dos-and-donts
	// Use the /all_resources_bound / D3DCOMPILE_ALL_RESOURCES_BOUND compile flag if possible This allows for the compiler to do a better job at optimizing texture accesses.
	// We have seen frame rate improvements of > 1 % when toggling this flag on.
	LPCWSTR Arguments[] =
	{
		L"-all_resources_bound",
#ifdef _DEBUG
		L"-WX",				// Warnings as errors
		L"-Zi",				// Debug info
		L"-Qembed_debug",	// Embed debug info into the shader
		L"-Od",				// Disable optimization
#else
		L"-O3",				// Optimization level 3
#endif
		// Add include directory
		L"-I", m_IncludeDirectory.data()
	};

	ComPtr<IDxcBlobEncoding>	pSource;
	ComPtr<IDxcIncludeHandler>	pDxcIncludeHandler;
	UINT32						CodePage = CP_ACP;

	ThrowIfFailed(m_DxcLibrary->CreateBlobFromFile(Path.c_str(), &CodePage, pSource.ReleaseAndGetAddressOf()));
	ThrowIfFailed(m_DxcLibrary->CreateIncludeHandler(pDxcIncludeHandler.ReleaseAndGetAddressOf()));

	ComPtr<IDxcOperationResult> pDxcOperationResult;
	ThrowIfFailed(m_DxcCompiler->Compile(
		pSource.Get(),
		Path.c_str(),
		pEntryPoint,
		pProfile,
		Arguments, ARRAYSIZE(Arguments),
		ShaderDefines.data(), ShaderDefines.size(),
		pDxcIncludeHandler.Get(),
		pDxcOperationResult.ReleaseAndGetAddressOf()));

	HRESULT hr;
	pDxcOperationResult->GetStatus(&hr);
	if (SUCCEEDED(hr))
	{
		ComPtr<IDxcBlob> pDxcBlob;
		pDxcOperationResult->GetResult(pDxcBlob.ReleaseAndGetAddressOf());
		return pDxcBlob;
	}
	else
	{
		ComPtr<IDxcBlobEncoding> pError;
		ComPtr<IDxcBlobEncoding> pError16;

		pDxcOperationResult->GetErrorBuffer(pError.ReleaseAndGetAddressOf());
		m_DxcLibrary->GetBlobAsUtf16(pError.Get(), pError16.ReleaseAndGetAddressOf());
		OutputDebugString((LPCWSTR)pError16->GetBufferPointer());
		throw std::exception("Failed to compile shader, check output window");
	}
}