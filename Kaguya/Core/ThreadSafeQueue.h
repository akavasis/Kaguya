#pragma once
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

template<typename T>
class ThreadSafeQueue
{
public:
	ThreadSafeQueue()
		: Empty(true),
		NumElements(0)
	{
	}
	ThreadSafeQueue(const ThreadSafeQueue&) = delete;
	ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

	bool pop(T& Item, int TimeOutMicroseconds)
	{
		std::unique_lock<std::mutex> lk(Mutex);

		while (Empty)
		{
			if (TimeOutMicroseconds >= 0)
			{
				bool timedOut = ConditionVariable.wait_for(lk,
					std::chrono::microseconds(TimeOutMicroseconds),
					std::bind(&ThreadSafeQueue::Empty, this));

				if (Empty || timedOut)
				{
					return false;
				}
			}
			else
			{
				ConditionVariable.wait(lk,
					std::bind(&ThreadSafeQueue::Empty, this));
			}
		}

		Item = Queue.front();
		Queue.pop();
		NumElements--;
		Empty = (Queue.size() == 0);

		return true;
	}

	void push(const T& Item)
	{
		std::scoped_lock lk(Mutex);
		Queue.push(Item);
		NumElements++;
		Empty = false;
		ConditionVariable.notify_one();
	}

	int32_t size() const
	{
		return NumElements.load();
	}

	bool empty() const
	{
		return Empty.load();
	}
private:
	std::atomic<bool> Empty;
	std::atomic<int32_t> NumElements;
	std::mutex Mutex;
	std::condition_variable ConditionVariable;
	std::queue<T> Queue;
};