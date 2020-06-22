#include "pch.h"
#include "ShaderManager.h"

ShaderManager::ShaderManager()
{
	m_Shaders[(int)Shader::Type::Vertex].resize((int)VS::VS_Count);
	m_Shaders[(int)Shader::Type::Hull].resize((int)HS::HS_Count);
	m_Shaders[(int)Shader::Type::Domain].resize((int)DS::DS_Count);
	m_Shaders[(int)Shader::Type::Geometry].resize((int)GS::GS_Count);
	m_Shaders[(int)Shader::Type::Pixel].resize((int)PS::PS_Count);
	m_Shaders[(int)Shader::Type::Compute].resize((int)CS::CS_Count);
}

ShaderManager::~ShaderManager()
{

}

void ShaderManager::AddShader(VS vs, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	m_Shaders[(int)Shader::Type::Vertex][(int)vs] = m_ShaderCompiler.LoadShader(Shader::Type::Vertex, pPath, pEntryPoint, ShaderDefines);
}

void ShaderManager::AddShader(HS hs, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	m_Shaders[(int)Shader::Type::Hull][(int)hs] = m_ShaderCompiler.LoadShader(Shader::Type::Hull, pPath, pEntryPoint, ShaderDefines);
}

void ShaderManager::AddShader(DS ds, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	m_Shaders[(int)Shader::Type::Domain][(int)ds] = m_ShaderCompiler.LoadShader(Shader::Type::Domain, pPath, pEntryPoint, ShaderDefines);
}

void ShaderManager::AddShader(GS gs, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	m_Shaders[(int)Shader::Type::Geometry][(int)gs] = m_ShaderCompiler.LoadShader(Shader::Type::Geometry, pPath, pEntryPoint, ShaderDefines);
}

void ShaderManager::AddShader(PS ps, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	m_Shaders[(int)Shader::Type::Pixel][(int)ps] = m_ShaderCompiler.LoadShader(Shader::Type::Pixel, pPath, pEntryPoint, ShaderDefines);
}

void ShaderManager::AddShader(CS cs, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	m_Shaders[(int)Shader::Type::Compute][(int)cs] = m_ShaderCompiler.LoadShader(Shader::Type::Compute, pPath, pEntryPoint, ShaderDefines);
}

const Shader* ShaderManager::QueryShader(VS vs) const
{
	return m_Shaders[(int)Shader::Type::Vertex][(int)vs].get();
}

const Shader* ShaderManager::QueryShader(HS hs) const
{
	return m_Shaders[(int)Shader::Type::Hull][(int)hs].get();
}

const Shader* ShaderManager::QueryShader(DS ds) const
{
	return m_Shaders[(int)Shader::Type::Domain][(int)ds].get();
}

const Shader* ShaderManager::QueryShader(GS gs) const
{
	return m_Shaders[(int)Shader::Type::Geometry][(int)gs].get();
}

const Shader* ShaderManager::QueryShader(PS ps) const
{
	return m_Shaders[(int)Shader::Type::Pixel][(int)ps].get();
}

const Shader* ShaderManager::QueryShader(CS cs) const
{
	return m_Shaders[(int)Shader::Type::Compute][(int)cs].get();
}

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

std::unique_ptr<Shader> ShaderCompiler::LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	std::filesystem::path filePath = std::filesystem::absolute(pPath);
	if (!std::filesystem::exists(filePath))
	{
		throw std::exception("File Not Found");
	}

	std::vector<std::wstring> cmdlineArgs;
	cmdlineArgs.push_back(L"/all_resources_bound");

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
		cmdlineArgsPtr.data(),                // compilation arguments
		cmdlineArgsPtr.size(),                // number of compilation arguments
		nullptr, 0,
		pDxcIncludeHandler.Get(),
		pDxcOperationResult.ReleaseAndGetAddressOf()));

	HRESULT hr;
	pDxcOperationResult->GetStatus(&hr);
	if (SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<IDxcBlob> pDxcBlob;
		pDxcOperationResult->GetResult(pDxcBlob.ReleaseAndGetAddressOf());
		return std::make_unique<Shader>(Type, pDxcBlob);
	}
	else
	{
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pError;
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> pError16;

		pDxcOperationResult->GetErrorBuffer(pError.ReleaseAndGetAddressOf());
		m_DxcLibrary->GetBlobAsUtf16(pError.Get(), pError16.ReleaseAndGetAddressOf());
		OutputDebugString((LPWSTR)pError16->GetBufferPointer());
		return std::make_unique<Shader>(Type, nullptr);
	}
}

Shader::Shader(Type Type, Microsoft::WRL::ComPtr<IDxcBlob> DxcBlob)
	: m_Type(Type),
	m_DxcBlob(DxcBlob)
{
}

Shader::~Shader()
{
}