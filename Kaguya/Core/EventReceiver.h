#pragma once
#include "Event.h"
#include <vector>

// Credit: Lari Norri
class EventDispatcher;
class EventReceiver
{
public:
	EventReceiver();
	virtual ~EventReceiver() = default;

	const EventReceiverID& QueryID() const;
	void SetName(const std::string& name);
	bool Register(EventDispatcher& eventDispatcher, const std::function<void(const Event&)>& callback);
	void Append(const Event& event);
	void Pop(Event& event);
	void Peek(Event& event);
	void Invoke(const Event& event) const;
private:
	unsigned int m_EventMissedCount;
	bool m_EventWaiting;
	Event m_EurrentEvent;
	EventReceiverID m_ID;
	std::vector<std::function<void(const Event&)>> m_Callbacks;
};