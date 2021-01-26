#pragma once
#include "RenderDevice.h"

class ToneMapper
{
public:
	enum Operator
	{
		ACES
	};

	void Create(RenderDevice* pRenderDevice);

	// Apply tone mapping to input shader resource into output render target
	void Apply(Descriptor ShaderResourceView, Descriptor RenderTargetView, CommandList& CommandList);
private:
	RenderDevice* pRenderDevice;

	RootSignature RS;
	PipelineState PSO;
};