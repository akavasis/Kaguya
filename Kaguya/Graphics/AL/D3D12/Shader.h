#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <dxcapi.h>

class Shader
{
public:
	enum class Type
	{
		Unknown, Vertex, Hull, Domain, Geometry, Pixel, Compute
	};

	Shader() = default;
	Shader(Type Type, Microsoft::WRL::ComPtr<IDxcBlob> DxcBlob);
	~Shader() = default;

	Shader(Shader&&) noexcept = default;
	Shader& operator=(Shader&&) noexcept = default;

	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;

	inline auto GetType() const { return m_Type; }
	inline auto GetDxcBlob() const { return m_DxcBlob.Get(); }
	inline D3D12_SHADER_BYTECODE GetD3DShaderBytecode() const { return { m_DxcBlob->GetBufferPointer(), m_DxcBlob->GetBufferSize() }; }
private:
	Type m_Type = Shader::Type::Unknown;
	Microsoft::WRL::ComPtr<IDxcBlob> m_DxcBlob;
};