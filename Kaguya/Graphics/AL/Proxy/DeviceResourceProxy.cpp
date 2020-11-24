#include "pch.h"
#include "DeviceResourceProxy.h"

DeviceResourceProxy::DeviceResourceProxy(Resource::Type Type)
	: m_Type(Type),
	m_NumSubresources(0),
	m_ClearValue(std::nullopt)
{
	BindFlags = Resource::BindFlags::None;
	InitialState = Resource::State::Unknown;
}

void DeviceResourceProxy::Link()
{
	assert(InitialState != Resource::State::Unknown);
	assert(m_Type != Resource::Type::Unknown);
}