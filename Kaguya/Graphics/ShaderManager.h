#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <dxcapi.h>

#include <vector>
#include <string>
#include <filesystem>

enum class VS : int
{
	VS_Default,
	VS_Quad,
	VS_Sky,
	VS_Shadow,
	VS_Count
};

enum class HS : int
{
	HS_Default,
	HS_Count
};

enum class DS : int
{
	DS_Default,
	DS_Count
};

enum class GS : int
{
	GS_Default,
	GS_Count
};

enum class PS : int
{
	PS_Default,
	PS_BlinnPhong,
	PS_PBR,
	PS_Sepia,
	PS_SobelComposite,
	PS_BloomMask,
	PS_BloomComposite,
	PS_ToneMapping,
	PS_Sky,
	PS_IrradianceConvolution,
	PS_PrefilterConvolution,
	PS_BRDFIntegration,
	PS_Count
};

enum class CS : int
{
	CS_HorizontalBlur,
	CS_VerticalBlur,
	CS_Sobel,
	CS_EquirectangularToCubeMap,
	CS_GenerateMips,
	CS_Count
};

class Shader
{
public:
	enum class Type
	{
		Vertex, Hull, Domain, Geometry, Pixel, Compute, NumShaderTypes
	};

	enum class Model
	{
		Model_6_3,
		Model_6_4
	};

	Shader(Type Type, Microsoft::WRL::ComPtr<IDxcBlob> DxcBlob);
	~Shader();

	inline auto GetType() const { return m_Type; }
	inline auto GetDxcBlob() const { return m_DxcBlob.Get(); }
	inline D3D12_SHADER_BYTECODE GetD3DShaderBytecode() const { return { m_DxcBlob->GetBufferPointer(), m_DxcBlob->GetBufferSize() }; }
private:
	Type m_Type;
	Microsoft::WRL::ComPtr<IDxcBlob> m_DxcBlob;
};

std::wstring ProfileString(Shader::Type Type, Shader::Model Model);

class ShaderCompiler
{
public:
	ShaderCompiler();
	~ShaderCompiler();

	std::unique_ptr<Shader> LoadShader(Shader::Type Type, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
private:
	Microsoft::WRL::ComPtr<IDxcCompiler> m_DxcCompiler;
	Microsoft::WRL::ComPtr<IDxcLibrary> m_DxcLibrary;
};

class ShaderManager
{
public:
	ShaderManager();
	~ShaderManager();

	void AddShader(VS vs, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	void AddShader(HS hs, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	void AddShader(DS ds, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	void AddShader(GS gs, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	void AddShader(PS ps, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);
	void AddShader(CS cs, LPCWSTR pPath, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines);

	const Shader* QueryShader(VS vs) const;
	const Shader* QueryShader(HS hs) const;
	const Shader* QueryShader(DS ds) const;
	const Shader* QueryShader(GS gs) const;
	const Shader* QueryShader(PS ps) const;
	const Shader* QueryShader(CS cs) const;
private:
	ShaderCompiler m_ShaderCompiler;
	std::vector<std::unique_ptr<Shader>> m_Shaders[(int)Shader::Type::NumShaderTypes];
};