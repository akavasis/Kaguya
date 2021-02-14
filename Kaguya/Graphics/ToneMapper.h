#pragma once
#include "RenderDevice.h"

class ToneMapper
{
public:
	enum Operator
	{
		ACES
	};

	void Create();

	void SetResolution(UINT Width, UINT Height);

	void Apply(Descriptor ShaderResourceView, CommandList& CommandList);

	auto GetSRV() const
	{
		return SRV;
	}

	auto GetRenderTarget() const
	{
		return m_RenderTarget->pResource.Get();
	}
private:
	UINT Width = 0, Height = 0;

	Descriptor RTV;
	Descriptor SRV;

	std::shared_ptr<Resource> m_RenderTarget;

	RootSignature RS;
	PipelineState PSO;
};