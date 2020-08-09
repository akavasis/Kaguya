#pragma once
#include <compare>

#define HANDLE_MAX_BITFIELD_SIZE (sizeof(size_t) * 8)

template<typename Type, size_t TypeBitField, typename Flags, size_t FlagsBitField>
struct Handle
{
	Handle()
		: Data(0)
	{
	}
	auto operator<=>(const Handle<Type, TypeBitField, Flags, FlagsBitField>&) const = default;
	operator bool() const
	{
		return Data != 0;
	}

	Type Type : TypeBitField;
	Flags Flags : FlagsBitField;
	size_t Data : HANDLE_MAX_BITFIELD_SIZE - TypeBitField - FlagsBitField;
};

#define DECL_HANDLE(Name, Type, TypeBitField, Flags, FlagsBitField)					\
typedef Handle<Type, TypeBitField, Flags, FlagsBitField> Name;						\
static_assert(sizeof(Name) == sizeof(size_t));										\
struct Name##TypeMaxSize { static constexpr size_t Value = 1 << TypeBitField; };	\
struct Name##FlagsMaxSize { static constexpr size_t Value = 1 << FlagsBitField; };