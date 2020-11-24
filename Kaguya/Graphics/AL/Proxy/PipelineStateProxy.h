#pragma once
#include <d3d12.h>
#include "Proxy.h"
#include "../BlendState.h"
#include "../RasterizerState.h"
#include "../DepthStencilState.h"
#include "../InputLayout.h"
#include "../D3D12/PipelineState.h"

class RootSignature;
class Shader;

class PipelineStateProxy : public Proxy
{
public:
	friend class PipelineState;
	PipelineStateProxy(PipelineState::Type Type);

	const RootSignature* pRootSignature;	// Default value: nullptr, must be set
protected:
	void Link() override;

	PipelineState::Type m_Type;				// Default value: set by ctor
private:
	D3D12_PIPELINE_STATE_STREAM_DESC BuildD3DDesc();
};

class GraphicsPipelineStateProxy : public PipelineStateProxy
{
public:
	friend class GraphicsPipelineState;
	GraphicsPipelineStateProxy();

	const Shader* pVS;						// Default value: nullptr, optional set
	const Shader* pPS;						// Default value: nullptr, optional set
	const Shader* pDS;						// Default value: nullptr, optional set
	const Shader* pHS;						// Default value: nullptr, optional set
	const Shader* pGS;						// Default value: nullptr, optional set
	BlendState BlendState;					// Default value: Default, optional set
	RasterizerState RasterizerState;		// Default value: Default, optional set
	DepthStencilState DepthStencilState;	// Default value: Default, optional set
	InputLayout InputLayout;				// Default value: Default, optional set
	PrimitiveTopology PrimitiveTopology;	// Default value: Undefined, must be set

	void AddRenderTargetFormat(DXGI_FORMAT Format);
	void SetDepthStencilFormat(DXGI_FORMAT Format);
	void SetSampleDesc(UINT Count, UINT Quality);
protected:
	void Link() override;
private:
	D3D12_GRAPHICS_PIPELINE_STATE_DESC BuildD3DDesc();

	UINT m_NumRenderTargets;		// Default value: 0, optional set
	DXGI_FORMAT m_RTVFormats[8];	// Default value: DXGI_FORMAT_UNKNOWN, optional set
	DXGI_FORMAT m_DSVFormat;		// Default value: DXGI_FORMAT_UNKNOWN, optional set
	DXGI_SAMPLE_DESC m_SampleDesc;	// Default value: Count: 1, Quality: 0
};

class ComputePipelineStateProxy : public PipelineStateProxy
{
public:
	friend class ComputePipelineState;
	ComputePipelineStateProxy();

	const Shader* pCS;	// Default value: nullptr, must be set
protected:
	void Link() override;
private:
	D3D12_COMPUTE_PIPELINE_STATE_DESC BuildD3DDesc();
};