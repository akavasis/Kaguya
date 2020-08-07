#include "pch.h"
#include "RootSignatureProxy.h"

RootSignatureProxy::RootSignatureProxy()
{
	m_Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
}

void RootSignatureProxy::AddRootConstantsParameter(UINT ShaderRegister, UINT RegisterSpace, UINT Num32BitValues, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter = {};
	parameter.InitAsConstants(Num32BitValues, ShaderRegister, RegisterSpace, ShaderVisibility.value_or(D3D12_SHADER_VISIBILITY_ALL));

	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(parameter);
	// and indicate that there will be no range
	// location to indicate since this parameter is not part of the heap
	m_DescriptorRangeIndices.push_back(~0);
}

void RootSignatureProxy::AddRootCBVParameter(UINT ShaderRegister, UINT RegisterSpace, std::optional<D3D12_ROOT_DESCRIPTOR_FLAGS> Flags, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter = {};
	parameter.InitAsConstantBufferView(ShaderRegister, RegisterSpace, Flags.value_or(D3D12_ROOT_DESCRIPTOR_FLAG_NONE), ShaderVisibility.value_or(D3D12_SHADER_VISIBILITY_ALL));

	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(parameter);
	// and indicate that there will be no range
	// location to indicate since this parameter is not part of the heap
	m_DescriptorRangeIndices.push_back(~0);
}

void RootSignatureProxy::AddRootSRVParameter(UINT ShaderRegister, UINT RegisterSpace, std::optional<D3D12_ROOT_DESCRIPTOR_FLAGS> Flags, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter = {};
	parameter.InitAsShaderResourceView(ShaderRegister, RegisterSpace, Flags.value_or(D3D12_ROOT_DESCRIPTOR_FLAG_NONE), ShaderVisibility.value_or(D3D12_SHADER_VISIBILITY_ALL));

	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(parameter);
	// and indicate that there will be no range
	// location to indicate since this parameter is not part of the heap
	m_DescriptorRangeIndices.push_back(~0);
}

void RootSignatureProxy::AddRootUAVParameter(UINT ShaderRegister, UINT RegisterSpace, std::optional<D3D12_ROOT_DESCRIPTOR_FLAGS> Flags, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter = {};
	parameter.InitAsUnorderedAccessView(ShaderRegister, RegisterSpace, Flags.value_or(D3D12_ROOT_DESCRIPTOR_FLAG_NONE), ShaderVisibility.value_or(D3D12_SHADER_VISIBILITY_ALL));

	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(parameter);
	// and indicate that there will be no range
	// location to indicate since this parameter is not part of the heap
	m_DescriptorRangeIndices.push_back(~0);
}

void RootSignatureProxy::AddRootDescriptorTableParameter(std::vector<D3D12_DESCRIPTOR_RANGE1> Ranges, std::optional<D3D12_SHADER_VISIBILITY> ShaderVisibility)
{
	CD3DX12_ROOT_PARAMETER1 parameter = {};
	// The range pointer is kept null here, and will be resolved when generating the root signature
	parameter.InitAsDescriptorTable(Ranges.size(), nullptr, ShaderVisibility.value_or(D3D12_SHADER_VISIBILITY_ALL));

	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(parameter);
	// The descriptor table descriptor ranges require a pointer to the descriptor ranges. Since new
	// ranges can be dynamically added in the vector, we separately store the index of the range set.
	// The actual address will be solved when generating the actual root signature
	m_DescriptorRangeIndices.push_back(m_DescriptorRanges.size());
	m_DescriptorRanges.push_back(std::move(Ranges));
}

void RootSignatureProxy::AddStaticSampler(UINT ShaderRegister,
	D3D12_FILTER Filter,
	D3D12_TEXTURE_ADDRESS_MODE AddressUVW,
	UINT MaxAnisotropy,
	D3D12_COMPARISON_FUNC ComparisonFunc,
	D3D12_STATIC_BORDER_COLOR BorderColor)
{
	CD3DX12_STATIC_SAMPLER_DESC staticSampler = {};
	staticSampler.Init(ShaderRegister, Filter, AddressUVW, AddressUVW, AddressUVW, 0.0f, MaxAnisotropy, ComparisonFunc, BorderColor);

	m_StaticSamplers.push_back(staticSampler);
}

void RootSignatureProxy::AllowInputLayout()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

void RootSignatureProxy::DenyVSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::DenyHSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::DenyDSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::DenyTessellationShaderAccess()
{
	DenyHSAccess();
	DenyDSAccess();
}

void RootSignatureProxy::DenyGSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::DenyPSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::AllowStreamOutput()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;
}

void RootSignatureProxy::SetAsLocalRootSignature()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
}

void RootSignatureProxy::Link()
{
	// Go through all the parameters, and set the actual addresses of the heap range descriptors based
	// on their indices in the range indices vector
	for (size_t i = 0; i < m_Parameters.size(); ++i)
	{
		if (m_Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			m_Parameters[i].DescriptorTable.pDescriptorRanges = m_DescriptorRanges[m_DescriptorRangeIndices[i]].data();
		}
	}

#ifdef _DEBUG
	// Used for validation
	struct ShaderRegister
	{
		UINT Register;
		UINT Space;

		auto operator<=>(const ShaderRegister&) const = default;
	};

	struct ShaderRegisterHash
	{
		size_t operator()(const ShaderRegister& shaderRegister) const
		{
			size_t hashValue = 0;
			hashValue |= shaderRegister.Register;
			hashValue <<= std::numeric_limits<decltype(shaderRegister.Register)>::digits;
			hashValue |= shaderRegister.Space;
			hashValue <<= std::numeric_limits<decltype(shaderRegister.Space)>::digits;
			return hashValue;
		}
	};

	// Key: Index in parameter, 
	std::unordered_map<ShaderRegister, UINT, ShaderRegisterHash> uniqueShaderRegisters;
	for (size_t i = 0; i < m_Parameters.size(); ++i)
	{
		std::vector<ShaderRegister> shaderRegisters;

		switch (m_Parameters[i].ParameterType)
		{
		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
		{
			const auto& descriptorTable = m_Parameters[i].DescriptorTable;
			for (size_t j = 0; j < descriptorTable.NumDescriptorRanges; ++j)
			{
				const auto& descriptorRange = descriptorTable.pDescriptorRanges[j];

				ShaderRegister shaderRegister;
				shaderRegister.Register = descriptorRange.BaseShaderRegister;
				shaderRegister.Space = descriptorRange.RegisterSpace;
				shaderRegisters.push_back(shaderRegister);
			}
		}
		break;

		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
		{
			ShaderRegister shaderRegister;
			shaderRegister.Register = m_Parameters[i].Constants.ShaderRegister;
			shaderRegister.Space = m_Parameters[i].Constants.RegisterSpace;
			shaderRegisters.push_back(shaderRegister);
		}
		break;

		case D3D12_ROOT_PARAMETER_TYPE_CBV: [[fallthrough]];
		case D3D12_ROOT_PARAMETER_TYPE_SRV: [[fallthrough]];
		case D3D12_ROOT_PARAMETER_TYPE_UAV:
		{
			ShaderRegister shaderRegister;
			shaderRegister.Register = m_Parameters[i].Descriptor.ShaderRegister;
			shaderRegister.Space = m_Parameters[i].Descriptor.RegisterSpace;
			shaderRegisters.push_back(shaderRegister);
		}
		break;

		default:
			break;
		}

		for (const auto& shaderRegister : shaderRegisters)
		{
			const auto [iter, success] = uniqueShaderRegisters.emplace(shaderRegister, UINT(i));
			if (!success)
			{
				assert("A shader register already occupies in this RootParameter index");
			}
		}
	}
#endif
}

D3D12_ROOT_SIGNATURE_DESC1 RootSignatureProxy::BuildD3DDesc()
{
	D3D12_ROOT_SIGNATURE_DESC1 desc = {};
	desc.NumParameters = static_cast<UINT>(m_Parameters.size());
	desc.pParameters = m_Parameters.data();
	desc.NumStaticSamplers = static_cast<UINT>(m_StaticSamplers.size());
	desc.pStaticSamplers = m_StaticSamplers.data();
	desc.Flags = m_Flags;

	return desc;
}