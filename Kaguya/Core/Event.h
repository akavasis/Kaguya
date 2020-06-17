#pragma once
#include "InterfaceID.h"
#include <memory>

// Credit: Lari Norri
/*
	This event system is based on Lari's new event system in the new Gateware Architecture.
	Porting it to my own project because it is extremely easy to use, allowing flexible data transfer
	between different events. Limitations are event data blocks are 32 bytes
*/
class Event
{
public:
	Event();
	~Event() = default;

	template<class Enum, class Data>
	bool Write(const Enum& EnumValue, const Data& DataValue)
	{
		static_assert(std::is_enum<Enum>::value, "Argument 1 is not an enum!");
		static_assert(std::is_trivially_copyable<Data>::value, "Argument 2 is not a POD");
		static_assert(sizeof(Enum) < EventSize, "Size of Enum exceeds EVENT_SIZE");
		static_assert(sizeof(Data) < EventSize, "Size of Data exceeds EVENT_SIZE");
		static_assert(sizeof(Enum) + sizeof(Data) <= RawPacketSize, "Size of Enum + Size of Data exceeds RAW_DATA_SIZE");
		EnumTypeInfo = &typeid(Enum);
		DataTypeInfo = &typeid(Data);
		if (!EnumTypeInfo ||
			!DataTypeInfo)
			return false;
		RawData[0] = sizeof(Enum);
		std::memcpy(&RawData[1], &EnumValue, sizeof(Enum));
		RawData[sizeof(Enum) + 1] = sizeof(Data);
		std::memcpy(&RawData[sizeof(Enum) + 2], &DataValue, sizeof(Data));
		return true;
	}

	template<class Enum, class Data>
	bool Read(Enum& EnumValue, Data& DataValue) const
	{
		static_assert(std::is_enum<Enum>::value, "Argument 1 is not an enum!");
		static_assert(std::is_trivially_copyable<Data>::value, "Argument 2 is not a POD");
		static_assert(sizeof(Enum) < EventSize, "Size of Enum exceeds EVENT_SIZE");
		static_assert(sizeof(Data) < EventSize, "Size of Data exceeds EVENT_SIZE");
		static_assert(sizeof(Enum) + sizeof(Data) <= RawPacketSize, "Size of Enum + Size of Data exceeds RAW_DATA_SIZE");
		if (EnumTypeInfo != &typeid(Enum) ||
			DataTypeInfo != &typeid(Data))
			return false;
		if (RawData[0] == sizeof(Enum))
			std::memcpy((char*)&EnumValue, &RawData[1], sizeof(Enum));
		else
			return false;
		if (RawData[sizeof(Enum) + 1] == sizeof(Data))
			std::memcpy((char*)&DataValue, &RawData[sizeof(Enum) + 2], sizeof(Data));
		else
			return false;
		return true;
	}
private:
	static constexpr unsigned int EventSize = 32u;
	static constexpr unsigned int RawPacketSize = 32u - (sizeof(std::size_t) << 1u) + 2u;
	const std::type_info* EnumTypeInfo;
	const std::type_info* DataTypeInfo;
	unsigned char RawData[RawPacketSize];
};

struct EventReceiverID : public InterfaceID
{
	EventReceiverID() : InterfaceID(nullptr, "Unknown EventReceiver") {}
	EventReceiverID(const std::type_info* Type, const std::string& Name) : InterfaceID(Type, Name) {}
};