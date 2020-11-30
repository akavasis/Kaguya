#pragma once
#include <unordered_map>
#include "API/D3D12/Texture.h"
#include "API/D3D12/DescriptorHeap.h"

class RenderTexture
{
public:
	Texture* pTexture;
	std::unordered_map<UINT64, Descriptor> ShaderResourceViews;
	std::unordered_map<UINT64, Descriptor> UnorderedAccessViews;
	std::unordered_map<UINT64, Descriptor> RenderTargetViews;
	std::unordered_map<UINT64, Descriptor> DepthStencilViews;
};