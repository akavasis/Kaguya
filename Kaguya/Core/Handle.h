#pragma once
#include <compare>

// 64 bits
static constexpr size_t HANDLE_MAX_BITFIELD_SIZE = 64;

template<typename Type_t, size_t TypeBitField, typename Flags_t, size_t FlagsBitField>
struct Handle
{
	Handle()
		: Type(Type_t()),
		Flags(Flags_t()),
		Data(0)
	{
	}
	auto operator<=>(const Handle<Type_t, TypeBitField, Flags_t, FlagsBitField>&) const = default;
	operator bool() const
	{
		return Type != Type_t() && Flags != Flags_t() && Data != 0;
	}

	Type_t	Type	: TypeBitField;
	Flags_t	Flags	: FlagsBitField;
	size_t	Data	: HANDLE_MAX_BITFIELD_SIZE - TypeBitField - FlagsBitField;
};

#define DECL_HANDLE(Name, Type, TypeBitField, Flags, FlagsBitField)					\
typedef Handle<Type, TypeBitField, Flags, FlagsBitField> Name;						\
static_assert(sizeof(Name) == sizeof(size_t));										\
struct Name##TypeMaxSize { static constexpr size_t Value = 1 << TypeBitField; };	\
struct Name##FlagsMaxSize { static constexpr size_t Value = 1 << FlagsBitField; };