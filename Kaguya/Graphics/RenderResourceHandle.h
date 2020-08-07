#pragma once
#include "Core/Handle.h"

#define RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD (size_t(4))
#define RENDER_RESOURCE_HANDLE_FLAGS_BIT_FIELD (size_t(8))
#define RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD (size_t(52))

enum class RenderResourceType : size_t
{
	Unknown,
	Shader,
	Library,
	Buffer,
	Texture,
	Heap,
	RootSignature,
	GraphicsPSO,
	ComputePSO,
	RaytracingPSO
};

enum class RenderResourceFlags : size_t
{
	Inactive = 0,
	Active = 1 << 0,
	Destroyed = 1 << 1
};

DECL_HANDLE(RenderResourceHandle, RenderResourceType, RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD, RenderResourceFlags, RENDER_RESOURCE_HANDLE_FLAGS_BIT_FIELD)

namespace std
{
	// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x 
	template <typename T>
	inline void hash_combine(size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

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