#include "pch.h"
#include "Shader.h"

Shader::Shader(Type Type, Microsoft::WRL::ComPtr<IDxcBlob> DxcBlob)
	: m_Type(Type),
	m_DxcBlob(DxcBlob)
{
}

Shader::~Shader()
{
}