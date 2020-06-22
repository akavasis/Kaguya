#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct RenderResourceHandle
{
	enum Type
	{
		GraphicsPSO,
		ComputePSO,
		RootSignature,

		Buffer,
		Texture1D,
		Texture2D,
		Texture3D,
	};

	enum Flag
	{
		INACTIVE = 0b00000001,
		DESTROY = 0b00000010
	};

	UINT64 _Type : 4;
	UINT64 _Flag : 8;
	UINT64 Data : 52;
};

void SetRenderHandleType(RenderResourceHandle& RenderHandle, RenderResourceHandle::Type Type)
{
	RenderHandle._Type = Type;
}

void SetRenderHandleFlag(RenderResourceHandle& RenderHandle, RenderResourceHandle::Flag Flag)
{
	RenderHandle._Flag = Flag;
}

#define RENDER_HANDLE_MAX_DATA_SIZE (1 << 54)

namespace std
{
	// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x 
	template <typename T>
	inline void hash_combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	template<>
	struct hash<RenderResourceHandle>
	{
		std::size_t operator()(const RenderResourceHandle& renderResourceHandle) const noexcept
		{
			std::size_t seed = 0;
			hash_combine(seed, renderResourceHandle._Type);
			hash_combine(seed, renderResourceHandle.Data);
			return seed;
		}
	};
}