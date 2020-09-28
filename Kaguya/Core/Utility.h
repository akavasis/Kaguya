#pragma once
#include <type_traits>
// http://blog.bitwigglers.org/using-enum-classes-as-type-safe-bitmasks/

template<typename Enum>
struct EnableBitMaskOperators
{
	static const bool Enable = false;
};

template<typename Enum>
typename std::enable_if<EnableBitMaskOperators<Enum>::Enable, Enum>::type
operator|(Enum lhs, Enum rhs)
{
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOperators<Enum>::Enable, Enum>::type
operator&(Enum lhs, Enum rhs)
{
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOperators<Enum>::Enable, Enum>::type
operator^(Enum lhs, Enum rhs)
{
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOperators<Enum>::Enable, Enum>::type
operator~(Enum rhs)
{
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum>(~static_cast<underlying>(rhs));
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOperators<Enum>::Enable, Enum&>::type
operator|=(Enum& lhs, Enum rhs)
{
	using underlying = typename std::underlying_type<Enum>::type;
	lhs = static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
	return lhs;
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOperators<Enum>::Enable, Enum&>::type
operator&=(Enum& lhs, Enum rhs)
{
	using underlying = typename std::underlying_type<Enum>::type;
	lhs = static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
	return lhs;
}

template<typename Enum>
typename std::enable_if<EnableBitMaskOperators<Enum>::Enable, Enum&>::type
operator^=(Enum& lhs, Enum rhs)
{
	using underlying = typename std::underlying_type<Enum>::type;
	lhs = static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
	return lhs;
}

#define ENABLE_BITMASK_OPERATORS(Enum)						\
template<>													\
struct EnableBitMaskOperators<Enum>							\
{															\
	static const bool Enable = true;						\
};															\
															\
inline bool EnumMaskBitSet(Enum Mask, Enum Component)		\
{															\
    return (Mask & Component) == Component;					\
}

#include <codecvt>
#include <string>

static inline std::wstring UTF8ToUTF16(const std::string& utf8Str)
{
	static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
	return conv.from_bytes(utf8Str);
}

static inline std::string UTF16ToUTF8(const std::wstring& utf16Str)
{
	static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
	return conv.to_bytes(utf16Str);
}