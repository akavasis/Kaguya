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
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_DxcCompiler.ReleaseAndGetAddressOf()));
	DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(m_DxcLibrary.ReleaseAndGetAddressOf()));
}

ShaderCompiler::~ShaderCompiler()
{

}

Shader ShaderCompiler::LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	std::filesystem::path filePath = std::filesystem::absolute(pPath);
	if (!std::filesystem::exists(filePath))
	{
		throw std::exception("File Not Found");
	}

	std::vector<std::wstring> cmdlineArgs;

#if defined(_DEBUG)
	cmdlineArgs.push_back(L"/Zi");
	cmdlineArgs.push_back(L"/Od");
#endif
	std::vector<LPCWSTR> cmdlineArgsPtr;
	for (auto& argument : cmdlineArgs)
		cmdlineArgsPtr.push_back(argument.c_str());

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
		cmdlineArgsPtr.data(), cmdlineArgsPtr.size(),
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
		CORE_ERROR("{} Failed: {}", __FUNCTION__, pError16->GetBufferPointer());
		return Shader(Shader::Type::Unknown, nullptr);
	}
}