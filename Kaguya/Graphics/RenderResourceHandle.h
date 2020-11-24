#pragma once
#include <compare>
#include "Core/Utility.h"

enum class RenderResourceType : UINT64
{
	Unknown,
	Buffer,
	Texture,
	Heap,
	RootSignature,
	GraphicsPSO,
	ComputePSO,
	RaytracingPSO
};

struct RenderResourceHandle
{
	RenderResourceHandle();

	auto operator<=>(const RenderResourceHandle&) const = default;

	operator bool() const;

	RenderResourceType	Type	: 4ull;
	UINT64				Data	: 60ull;
};

static_assert(sizeof(RenderResourceHandle) == sizeof(UINT64));

namespace std
{
	template<>
	struct hash<RenderResourceHandle>
	{
		size_t operator()(const RenderResourceHandle& RenderResourceHandle) const noexcept
		{
			std::size_t seed = 0;
			hash_combine(seed, size_t(RenderResourceHandle.Type));
			hash_combine(seed, size_t(RenderResourceHandle.Data));
			return seed;
		}
	};
}