#pragma once
#include "Core/Handle.h"
#include "Core/Utility.h"

#define RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD (size_t(4))
#define RENDER_RESOURCE_HANDLE_FLAGS_BIT_FIELD (size_t(8))
#define RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD (size_t(52))

enum class RenderResourceType : size_t
{
	Unknown,
	DeviceBuffer,
	DeviceTexture,
	Heap,
	RootSignature,
	GraphicsPSO,
	ComputePSO,
	RaytracingPSO
};

enum class RenderResourceFlags : size_t
{
	Inactive	= 0,
	Active		= 1 << 0,
	Destroyed	= 1 << 1
};

DECL_HANDLE(RenderResourceHandle, RenderResourceType, RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD, RenderResourceFlags, RENDER_RESOURCE_HANDLE_FLAGS_BIT_FIELD)

namespace std
{
	template<>
	struct hash<RenderResourceHandle>
	{
		size_t operator()(const RenderResourceHandle& renderResourceHandle) const noexcept
		{
			std::size_t seed = 0;
			hash_combine(seed, size_t(renderResourceHandle.Type));
			hash_combine(seed, size_t(renderResourceHandle.Flags));
			hash_combine(seed, size_t(renderResourceHandle.Data));
			return seed;
		}
	};
}