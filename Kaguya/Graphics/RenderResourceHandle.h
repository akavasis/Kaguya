#pragma once
#include <functional>

#define RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD (4)
#define RENDER_RESOURCE_HANDLE_FLAG_BIT_FIELD (8)
#define RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD (52)
// Ensure handle size is 64 bits
static_assert(RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD + RENDER_RESOURCE_HANDLE_FLAG_BIT_FIELD + RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD == 64, "Bit field mismatch");

#define RENDER_RESOURCE_HANDLE_MAX_TYPE_SIZE (1 << (RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD))
#define RENDER_RESOURCE_HANDLE_MAX_FLAG_SIZE (1 << (RENDER_RESOURCE_HANDLE_FLAG_BIT_FIELD))
#define RENDER_RESOURCE_HANDLE_MAX_DATA_SIZE (1 << (RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD))

struct RenderResourceHandle
{
	enum class Type : std::size_t
	{
		Unknown,

		Shader,

		Buffer,
		Texture,

		Heap,
		RootSignature,
		GraphicsPSO,
		ComputePSO
	};

	enum class Flags : std::size_t
	{
		Inactive = 0,
		Active = 1 << 0,
		Destroyed = 1 << 1
	};

	RenderResourceHandle();
	RenderResourceHandle(Type Type, Flags Flag, std::size_t Data);

	bool operator==(const RenderResourceHandle& rhs) const;
	operator bool() const;

	Type _Type : RENDER_RESOURCE_HANDLE_TYPE_BIT_FIELD;
	Flags _Flag : RENDER_RESOURCE_HANDLE_FLAG_BIT_FIELD;
	std::size_t _Data : RENDER_RESOURCE_HANDLE_DATA_BIT_FIELD;
};

static_assert(sizeof(RenderResourceHandle) == sizeof(std::size_t), "Size mismatch");

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
			hash_combine(seed, std::size_t(renderResourceHandle._Type));
			hash_combine(seed, std::size_t(renderResourceHandle._Data));
			return seed;
		}
	};
}