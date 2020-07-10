#include "pch.h"
#include "RenderResourceHandle.h"

RenderResourceHandle::RenderResourceHandle()
	: _Type(Types::Unknown), _Flag(Flags::Inactive), _Data(0)
{
}

RenderResourceHandle::operator bool() const
{
	return _Type != Types::Unknown && _Flag != Flags::Inactive && _Data != 0;
}