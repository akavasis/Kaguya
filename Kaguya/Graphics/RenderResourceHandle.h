#pragma once

#include <basetsd.h>		// UINT64
#include <compare>			// operator<=>
#include "Core/Utility.h"	// hash_combine

enum class RenderResourceType : UINT64
{
	Unknown,
	Buffer,
	Texture,
	Heap,
	RootSignature,
	PipelineState,
	RaytracingPipelineState
};

constexpr auto MAX_UID = (1ull << 28ull);
constexpr auto MAX_PTR = (1ull << 32ull);

struct RenderResourceHandle
{
	RenderResourceHandle()
	{
		Type = RenderResourceType::Unknown;
		UID = MAX_UID;
		Ptr = MAX_PTR;
	}

	auto operator<=>(const RenderResourceHandle&) const = default;

	operator bool() const
	{
		return Type != RenderResourceType::Unknown && UID != MAX_UID && Ptr != MAX_PTR;
	}

	RenderResourceType	Type : 4ull;
	UINT64				UID : 28ull;
	UINT64				Ptr : 32ull;
};

static_assert(sizeof(RenderResourceHandle) == sizeof(UINT64));

namespace std
{
	template<>
	struct hash<RenderResourceHandle>
	{
		size_t operator()(const RenderResourceHandle& RenderResourceHandle) const noexcept
		{
			size_t seed = 0;
			hash_combine(seed, size_t(RenderResourceHandle.Type));
			hash_combine(seed, size_t(RenderResourceHandle.UID));
			hash_combine(seed, size_t(RenderResourceHandle.Ptr));
			return seed;
		}
	};
}