#pragma once
#include <d3d12.h>

class RootSignatureProxy
{
public:
	/*
		The maximum size of a root signature is 64 DWORDs.

		This maximum size is chosen to prevent abuse of the root signature as a way of storing bulk data. Each entry in the root signature has a cost towards this 64 DWORD limit:

		Descriptor tables cost 1 DWORD each.
		Root constants cost 1 DWORD each, since they are 32-bit values.
		Root descriptors (64-bit GPU virtual addresses) cost 2 DWORDs each.
		Static samplers do not have any cost in the size of the root signature.
	*/

	/*
		Use a small a root signature as necessary, though balance this with the flexibility of a larger root signature.
		Arrange parameters in a large root signature so that the parameters most likely to change often, or if low access latency for a given parameter is important, occur first.
		If convenient, use root constants or root constant buffer views over putting constant buffer views in a descriptor heap.
	*/
	RootSignatureProxy();
	~RootSignatureProxy() = default;

	void AddRootConstantsParameter(UINT ShaderRegister, UINT RegisterSpace, UINT Num32BitValues, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility = {});
	template<typename T>
	void AddRootConstantsParameter(UINT ShaderRegister, UINT RegisterSpace, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility = {})
	{
		static_assert(std::is_trivially_copyable<T>::value, "typename T must be trivially copyable!");
		static_assert(sizeof(T) % 4 == 0, "typename T must be 4 byte aligned");
		AddRootConstantsParameter(ShaderRegister, RegisterSpace, sizeof(T) / 4, ShaderVisibility);
	}
	void AddRootCBVParameter(UINT ShaderRegister, UINT RegisterSpace, std::optional<D3D12_ROOT_DESCRIPTOR_FLAGS> Flags = {}, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility= {});
	void AddRootSRVParameter(UINT ShaderRegister, UINT RegisterSpace, std::optional<D3D12_ROOT_DESCRIPTOR_FLAGS> Flags = {}, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility= {});
	void AddRootUAVParameter(UINT ShaderRegister, UINT RegisterSpace, std::optional<D3D12_ROOT_DESCRIPTOR_FLAGS> Flags = {}, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility= {});
	void AddRootDescriptorTableParameter(std::vector<D3D12_DESCRIPTOR_RANGE1> Ranges, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility = {});

	void AddStaticSampler(UINT ShaderRegister,
		D3D12_FILTER Filter,
		D3D12_TEXTURE_ADDRESS_MODE AddressUVW,
		UINT MaxAnisotropy,
		D3D12_COMPARISON_FUNC ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	void AllowInputLayout();
	void DenyVSAccess();
	void DenyHSAccess();
	void DenyDSAccess();
	void DenyTessellationShaderAccess();
	void DenyGSAccess();
	void DenyPSAccess();
	void AllowStreamOutput();
	void SetAsLocalRootSignature();
private:
	friend class RootSignature;
	void Link();

	std::vector<D3D12_ROOT_PARAMETER1> m_Parameters;
	std::vector<D3D12_STATIC_SAMPLER_DESC> m_StaticSamplers;
	D3D12_ROOT_SIGNATURE_FLAGS m_Flags;

	std::vector<UINT> m_DescriptorRangeIndices;
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE1>> m_DescriptorRanges;
};