#include "pch.h"
#include "RenderResourceHandle.h"
#include "RenderResourceHandleRegistry.h"

RenderResourceHandle::RenderResourceHandle()
	: Type(RenderResourceType::Unknown),
	Data(RenderResourceHandleRegistry::INVALID_HANDLE_DATA)
{
}

RenderResourceHandle::operator bool() const
{
	return Type != RenderResourceType::Unknown && Data != RenderResourceHandleRegistry::INVALID_HANDLE_DATA;
}