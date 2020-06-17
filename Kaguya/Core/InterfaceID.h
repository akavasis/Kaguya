#pragma once
#include <typeinfo> // std::type_info
#include <string> // std::string
#include <functional> // std::hash and std::tie

struct InterfaceID
{
	const std::type_info* Type;
	std::string Name;

	InterfaceID() : Type(nullptr), Name("Unknown Interface") {}
	InterfaceID(const std::type_info* Type, const std::string& Name) : Type(Type), Name(Name) {}
};

struct InterfaceIDHash
{
	virtual std::size_t operator()(const InterfaceID& rhs) const
	{
		return std::size_t(std::hash<std::string>()(rhs.Name) + rhs.Type->hash_code());
	}
};

struct InterfaceIDCompare
{
	virtual bool operator()(const InterfaceID& lhs, const InterfaceID& rhs) const
	{
		return std::tie(lhs.Type, lhs.Name) == std::tie(rhs.Type, rhs.Name);
	}
};