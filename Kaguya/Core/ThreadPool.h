#pragma once
#include <atomic>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <queue>

#include "Delegate.h"

namespace Kaguya
{
	namespace Internal
	{
		// RAII for threads
		class ThreadGuard
		{
		public:
			ThreadGuard(std::vector<std::thread>& Threads);
			~ThreadGuard();
		private:
			std::vector<std::thread>& m_RefThreads;
		};

		class WorkQueue
		{
		public:
			WorkQueue() = default;
			WorkQueue(const WorkQueue&) = delete;
			WorkQueue& operator=(const WorkQueue&) = delete;
			~WorkQueue() = default;

			void Enqueue(Delegate<void()> Work);
			bool Empty() const;
			bool TryPop(Delegate<void()>& Work);
			bool TrySteal(Delegate<void()>& Work);
		private:
			std::deque<Delegate<void()>> m_WorkQueue;
			mutable std::mutex m_Mutex;
		};
	}
}

class ThreadPool
{
public:
	ThreadPool(unsigned int NumThreads);
	~ThreadPool();

	template<typename Callable, typename... Args>
	decltype(auto) AddWork(Callable&& callable, Args&&... args)
	{
		using ReturnType = typename std::invoke_result<Callable>::type;

		std::packaged_task<ReturnType()> task(std::bind(std::forward<Callable>(callable), std::forward<Args>(args)...));

		std::future<ReturnType> future(task.get_future());
		if (pLocalQueue)
		{
			pLocalQueue->Enqueue(std::move(task));
		}
		else
		{
			std::scoped_lock lk(m_QueueMutex);
			m_GlobalWorkQueue.emplace(std::move(task));
		}
		m_QueueCondition.notify_one();
		return future;
	}
private:
	bool PopWorkFromLocalQueue(Delegate<void()>& Work);
	bool STEEEEEEEEAAAAAL(Delegate<void()>& Work);

	std::atomic_bool m_Done;
	std::mutex m_QueueMutex;
	std::condition_variable m_QueueCondition;
	std::queue<Delegate<void()>> m_GlobalWorkQueue;
	std::vector<std::unique_ptr<Kaguya::Internal::WorkQueue>> m_WorkQueuePool;

	std::vector<std::thread> m_Threads;
	Kaguya::Internal::ThreadGuard m_ThreadGuard;

	inline static thread_local Kaguya::Internal::WorkQueue* pLocalQueue;
	inline static thread_local unsigned int QueueIndex;
};