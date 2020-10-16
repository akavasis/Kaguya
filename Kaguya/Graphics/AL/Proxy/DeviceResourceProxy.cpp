#include "pch.h"
#include "DeviceResourceProxy.h"

DeviceResourceProxy::DeviceResourceProxy(DeviceResource::Type Type)
	: m_Type(Type),
	m_NumSubresources(0),
	m_ClearValue(std::nullopt)
{
	BindFlags = DeviceResource::BindFlags::None;
	InitialState = DeviceResource::State::Unknown;
}

void DeviceResourceProxy::Link()
{
	assert(InitialState != DeviceResource::State::Unknown);
	assert(m_Type != DeviceResource::Type::Unknown);
}