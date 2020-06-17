#include "pch.h"
#include "Event.h"

Event::Event()
{
	EnumTypeInfo = nullptr;
	DataTypeInfo = nullptr;
	std::memset(&RawData, 0, sizeof(RawData));
}