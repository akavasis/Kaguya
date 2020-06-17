#pragma once
#include "Event.h"
#include <unordered_map>

// Credit: Lari Norri
class EventReceiver;
class EventDispatcher
{
public:
	EventDispatcher() = default;
	virtual ~EventDispatcher() = default;

	bool Register(EventReceiver& receiver, const std::function<void(const Event&)>& callback);
	bool Deregister(EventReceiver& receiver);
	void Dispatch(const Event& event);
	unsigned int ObserverCount() const;
protected:
	Event m_Event;
private:
	struct EventReceiverHash { size_t operator()(const EventReceiver* rhs) const; };
	struct EventReceiverCompare { bool operator()(const EventReceiver* lhs, const EventReceiver* rhs) const; };

	// Receivers are only allowed to respond to 1 callback, but it may contain multiple callbacks so
	// different Dispatchers can call the one they would actually want the Receiver to respond
	std::unordered_map<EventReceiver*, std::function<void(const Event&)>, EventReceiverHash, EventReceiverCompare> m_Receivers;
};