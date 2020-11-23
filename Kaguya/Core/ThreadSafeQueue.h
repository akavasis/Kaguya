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

	bool pop(T& Item, int Milliseconds)
	{
		ScopedCriticalSection SCS(CriticalSection);

		while (Queue.empty())
		{
			BOOL Result = ::SleepConditionVariableCS(&ConditionVariable, &CriticalSection, Milliseconds);
			if (Result == ERROR_TIMEOUT || Queue.empty())
				return false;
		}

		Item = Queue.front();
		Queue.pop();

		return true;
	}

	void push(const T& Item)
	{
		ScopedCriticalSection SCS(CriticalSection);
		Queue.push(Item);
		::WakeConditionVariable(&ConditionVariable);
	}

	size_t size() const
	{
		return Queue.size();
	}

	bool empty() const
	{
		return Queue.empty();
	}
private:
	CriticalSection		CriticalSection;
	ConditionVariable	ConditionVariable;
	std::queue<T>		Queue;
};