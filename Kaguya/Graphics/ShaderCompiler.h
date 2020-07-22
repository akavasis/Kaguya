#pragma once
#include <dxcapi.h>
#include <wrl/client.h>

#include "Shader.h"

class ShaderCompiler
{
public:
	ShaderCompiler();
	~ShaderCompiler() = default;

	Shader LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
private:
	Microsoft::WRL::ComPtr<IDxcCompiler> m_DxcCompiler;
	Microsoft::WRL::ComPtr<IDxcLibrary> m_DxcLibrary;
};