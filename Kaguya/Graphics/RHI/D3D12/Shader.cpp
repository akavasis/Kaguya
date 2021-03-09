#include "pch.h"
#include "Shader.h"

Shader::Shader
(
	Type Type,
	Microsoft::WRL::ComPtr<IDxcBlob> DxcBlob,
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> ShaderReflection
)
	: m_Type(Type)
	, m_DxcBlob(DxcBlob)
	, m_ShaderReflection(ShaderReflection)
{

}