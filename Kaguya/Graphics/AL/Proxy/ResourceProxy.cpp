#include "pch.h"
#include "ResourceProxy.h"

ResourceProxy::ResourceProxy(Resource::Type Type)
	: m_Type(Type),
	m_NumSubresources(0),
	m_ClearValue(std::nullopt)
{
	BindFlags = Resource::BindFlags::None;
	InitialState = Resource::State::Unknown;
}

void ResourceProxy::Link()
{
	assert(InitialState != Resource::State::Unknown);
	assert(m_Type != Resource::Type::Unknown);
}