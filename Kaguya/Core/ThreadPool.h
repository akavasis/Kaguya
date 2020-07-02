#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <queue>
#include <functional>

class ThreadGuard
{
public:
	ThreadGuard(std::vector<std::thread>& Threads)
		: m_RefThreads(Threads)
	{}
	~ThreadGuard()
	{
		for (auto& thread : m_RefThreads)
		{
			if (thread.joinable())
				thread.join();
		}
	}

private:
	std::vector<std::thread>& m_RefThreads;
};

class ThreadPool
{
public:
	ThreadPool(unsigned int NumThreads)
		: m_Done(false),
		m_ThreadGuard(m_Threads)
	{
		auto threadCount = std::min(NumThreads, std::thread::hardware_concurrency());
		try
		{
			for (decltype(threadCount) i = 0; i < threadCount; ++i)
			{
				m_Threads.push_back(std::thread(
					[this]()
				{
					while (!m_Done)
					{
						std::function<void()> work;
						{
							std::unique_lock lk(m_Mutex);
							m_Condition.wait(lk, [this]() { return !m_WorkQueue.empty(); });
							work = std::move(m_WorkQueue.front());
							m_WorkQueue.pop();
						}
						if (work)
							work();
					}
				}
				));
			}
		}
		catch (...)
		{
			m_Done = true;
			throw;
		}
	}
	~ThreadPool()
	{
		m_Done = true;
	}



private:
	std::atomic_bool m_Done;
	std::vector<std::thread> m_Threads;
	ThreadGuard m_ThreadGuard;
	std::queue<std::function<void()>> m_WorkQueue;

	std::mutex m_Mutex;
	std::condition_variable m_Condition;
};