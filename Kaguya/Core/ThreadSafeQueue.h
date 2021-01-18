#pragma once

#include "Synchronization/CriticalSection.h"
#include "Synchronization/ConditionVariable.h"
#include <queue>

template<typename T>
class ThreadSafeQueue
{
public:
	ThreadSafeQueue() = default;
	ThreadSafeQueue(const ThreadSafeQueue&) = delete;
	ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

	void Enqueue(const T& Item)
	{
		ScopedCriticalSection SCS(CriticalSection);
		Queue.push(Item);
		::WakeConditionVariable(&ConditionVariable);
	}

	void Enqueue(T&& Item)
	{
		ScopedCriticalSection SCS(CriticalSection);
		Queue.push(std::move(Item));
		::WakeConditionVariable(&ConditionVariable);
	}

	bool Dequeue(T& Item, int Milliseconds)
	{
		ScopedCriticalSection SCS(CriticalSection);

		while (Queue.empty())
		{
			BOOL Result = ::SleepConditionVariableCS(&ConditionVariable, &CriticalSection, Milliseconds);
			if (Result == ERROR_TIMEOUT || Queue.empty())
				return false;
		}

		Item = std::move(Queue.front());
		Queue.pop();

		return true;
	}

	size_t Size() const
	{
		ScopedCriticalSection SCS(CriticalSection);
		return Queue.size();
	}

	bool IsEmpty() const
	{
		ScopedCriticalSection SCS(CriticalSection);
		return Queue.empty();
	}

private:
	mutable CriticalSection		CriticalSection;
	ConditionVariable			ConditionVariable;
	std::queue<T>				Queue;
};