#include "pch.h"
#include "RenderResourceHandle.h"

RenderResourceHandle::RenderResourceHandle()
	: _Type(Type::Unknown), _Flag(Flags::Inactive), _Data(0)
{
}

RenderResourceHandle::RenderResourceHandle(Type Type, Flags Flag, std::size_t Data)
	: _Type(Type), _Flag(Flag), _Data(Data)
{
}

bool RenderResourceHandle::operator==(const RenderResourceHandle& rhs) const
{
	return _Type == rhs._Type && _Flag == rhs._Flag && _Data == rhs._Data;
}

RenderResourceHandle::operator bool() const
{
	return _Type != Type::Unknown && _Flag != Flags::Inactive && _Data != 0;
}