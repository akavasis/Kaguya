#include "pch.h"
#include "EventDispatcher.h"
#include "EventReceiver.h"

size_t EventDispatcher::EventReceiverHash::operator()(const EventReceiver* rhs) const
{
	return InterfaceIDHash()(rhs->QueryID());
}

bool EventDispatcher::EventReceiverCompare::operator()(const EventReceiver* lhs, const EventReceiver* rhs) const
{
	return InterfaceIDCompare()(lhs->QueryID(), rhs->QueryID());
}

bool EventDispatcher::Register(EventReceiver& receiver, const std::function<void(const Event&)>& callback)
{
	auto iter = m_Receivers.find(&receiver);
	if (iter == m_Receivers.end())
	{
		m_Receivers[&receiver] = callback;
		return true;
	}
	return false;
}

bool EventDispatcher::Deregister(EventReceiver& receiver)
{
	auto iter = m_Receivers.find(&receiver);
	if (iter != m_Receivers.end())
	{
		m_Receivers.erase(iter);
		return true;
	}
	return false;
}

void EventDispatcher::Dispatch(const Event& event)
{
	if (m_Receivers.empty())
		return;
	for (auto iter = m_Receivers.begin(); iter != m_Receivers.end();)
	{
		if (!iter->second)
			iter = m_Receivers.erase(iter);
		else
		{
			iter->first->Append(event);
			iter->second(event);
			++iter;
		}
	}
}

unsigned int EventDispatcher::ObserverCount() const
{
	return static_cast<unsigned int>(m_Receivers.size());
}