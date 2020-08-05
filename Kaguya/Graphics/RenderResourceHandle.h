#pragma once
#include <functional>
#include <compare>

#define RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD (size_t(4))
#define RENDER_RESOURCE_HANDLE_FLAG_BIT_FIELD (size_t(8))
#define RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD (size_t(52))
// Ensure handle size is 64 bits
static_assert(RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD + RENDER_RESOURCE_HANDLE_FLAG_BIT_FIELD + RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD == 64, "Bit field mismatch");

#define RENDER_RESOURCE_HANDLE_MAX_TYPE_SIZE (size_t(1) << RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD)
#define RENDER_RESOURCE_HANDLE_MAX_FLAG_SIZE (size_t(1) << RENDER_RESOURCE_HANDLE_FLAG_BIT_FIELD)
#define RENDER_RESOURCE_HANDLE_MAX_DATA_SIZE (size_t(1) << RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD)

struct RenderResourceHandle
{
	enum class Types : size_t
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

	enum class Flags : size_t
	{
		Inactive = 0,
		Active = 1 << 0,
		Destroyed = 1 << 1
	};

	RenderResourceHandle()
		: Type(Types::Unknown), Flag(Flags::Inactive), Data(0)
	{
	}

	auto operator<=>(const RenderResourceHandle&) const = default;
	operator bool() const
	{
		return Type != Types::Unknown && Flag != Flags::Inactive && Data != 0;
	}

	Types Type : RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD;
	Flags Flag : RENDER_RESOURCE_HANDLE_FLAG_BIT_FIELD;
	size_t Data : RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD;
};
static_assert(sizeof(RenderResourceHandle) == sizeof(size_t), "Size mismatch");

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
			hash_combine(seed, size_t(renderResourceHandle.Flag));
			hash_combine(seed, size_t(renderResourceHandle.Data));
			return seed;
		}
	};
}