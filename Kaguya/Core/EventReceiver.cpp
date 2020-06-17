#include "pch.h"
#include "EventReceiver.h"
#include "EventDispatcher.h"

EventReceiver::EventReceiver()
{
	m_EventMissedCount = 0;
	m_EventWaiting = false;
	m_ID.Type = &typeid(*this);
}

const EventReceiverID& EventReceiver::QueryID() const
{
	return m_ID;
}

void EventReceiver::SetName(const std::string& name)
{
	m_ID.Name = name;
}

bool EventReceiver::Register(EventDispatcher& eventDispatcher, const std::function<void(const Event&)>& callback)
{
	m_Callbacks.push_back(callback);
	return eventDispatcher.Register(*this, callback);
}

void EventReceiver::Append(const Event& event)
{
	if (m_EventWaiting) { ++m_EventMissedCount; }
	m_EurrentEvent = event;
	m_EventWaiting = true;
}

void EventReceiver::Pop(Event& event)
{
	if (m_EventWaiting)
	{
		m_EventWaiting = false;
		event = m_EurrentEvent;
	}
}

void EventReceiver::Peek(Event& event)
{
	if (m_EventWaiting)
	{
		event = m_EurrentEvent;
	}
}

void EventReceiver::Invoke(const Event& event) const
{
	for (size_t i = 0; i < m_Callbacks.size(); ++i)
	{
		m_Callbacks[i](event);
	}
}