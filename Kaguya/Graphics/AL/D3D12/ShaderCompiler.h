#pragma once
#include <dxcapi.h>
#include <d3d12shader.h>
#include <wrl/client.h>

#include "Shader.h"
#include "Library.h"

class ShaderCompiler
{
public:
	enum class Profile
	{
		Profile_6_3,
		Profile_6_4,
		Profile_6_5
	};

	ShaderCompiler();
	~ShaderCompiler() = default;

	void SetIncludeDirectory(const std::filesystem::path& pPath);

	Shader CompileShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	Library CompileLibrary(LPCWSTR pPath);
private:
	Microsoft::WRL::ComPtr<IDxcBlob> Compile(LPCWSTR pPath, LPCWSTR pEntryPoint, LPCWSTR pProfile, const std::vector<DxcDefine>& ShaderDefines);

	Microsoft::WRL::ComPtr<IDxcCompiler>	m_DxcCompiler;
	Microsoft::WRL::ComPtr<IDxcLibrary>		m_DxcLibrary;
	Microsoft::WRL::ComPtr<IDxcUtils>		m_DxcUtils;
	std::wstring							m_IncludeDirectory;
};