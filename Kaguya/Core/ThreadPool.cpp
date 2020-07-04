#include "pch.h"
#include "ThreadPool.h"

namespace Kaguya
{
	namespace Internal
	{
		ThreadGuard::ThreadGuard(std::vector<std::thread>& Threads)
			: m_RefThreads(Threads)
		{
		}

		ThreadGuard::~ThreadGuard()
		{
			for (auto& thread : m_RefThreads)
			{
				if (thread.joinable())
					thread.join();
			}
		}

		void WorkQueue::Enqueue(Delegate<void()> Work)
		{
			std::scoped_lock lk(m_Mutex);
			m_WorkQueue.push_front(std::move(Work));
		}

		bool WorkQueue::Empty() const
		{
			std::scoped_lock lk(m_Mutex);
			return m_WorkQueue.empty();
		}

		bool WorkQueue::TryPop(Delegate<void()>& Work)
		{
			std::scoped_lock lk(m_Mutex);
			if (m_WorkQueue.empty())
				return false;
			Work = std::move(m_WorkQueue.front());
			m_WorkQueue.pop_front();
			return true;
		}

		bool WorkQueue::TrySteal(Delegate<void()>& Work)
		{
			std::scoped_lock lk(m_Mutex);
			if (m_WorkQueue.empty())
				return false;
			Work = std::move(m_WorkQueue.back());
			m_WorkQueue.pop_back();
			return true;
		}
	}
}

ThreadPool::ThreadPool(unsigned int NumThreads)
	: m_Done(false),
	m_ThreadGuard(m_Threads)
{
	NumThreads = std::max<unsigned int>(1, NumThreads);
	unsigned int threadCount = std::min<unsigned int>(NumThreads, std::thread::hardware_concurrency());
	try
	{
		for (unsigned int i = 0; i < threadCount; ++i)
		{
			m_WorkQueuePool.emplace_back(std::make_unique<Kaguya::Internal::WorkQueue>());
			m_Threads.emplace_back(std::thread(
				[this, i]()
			{
				QueueIndex = i;
				CORE_INFO("Thread Pool Worker: {} started working", QueueIndex);
				pLocalQueue = m_WorkQueuePool[QueueIndex].get();
				while (!m_Done)
				{
					Delegate<void()> work;
					{
						std::unique_lock lk(m_QueueMutex);
						m_QueueCondition.wait(lk, [this, &work = work]()
						{
							if (m_Done)
								return true;
							if (PopWorkFromLocalQueue(work))
							{
								return true;
							}
							else if (!m_GlobalWorkQueue.empty())
							{
								work = std::move(m_GlobalWorkQueue.front());
								m_GlobalWorkQueue.pop();
								return true;
							}
							else if (STEEEEEEEEAAAAAL(work))
							{
								return true;
							}
							return false;
						});
					}
					// A spurious wake up can happen so check to see if work is actually valid
					if (work)
					{
						CORE_INFO("Thread Pool Worker: {} got work to do", QueueIndex);
						work();
					}
				}
			}));
		}
	}
	catch (...)
	{
		m_Done = true;
		throw;
	}
}

ThreadPool::~ThreadPool()
{
	m_Done = true;
	m_QueueCondition.notify_all();
}

bool ThreadPool::PopWorkFromLocalQueue(Delegate<void()>& Work)
{
	return pLocalQueue && pLocalQueue->TryPop(Work);
}

bool ThreadPool::STEEEEEEEEAAAAAL(Delegate<void()>& Work)
{
	for (size_t i = 0, size = m_WorkQueuePool.size(); i < size; ++i)
	{
		size_t index = (QueueIndex + i + 1) % size;
		if (m_WorkQueuePool[i]->TrySteal(Work))
		{
			return true;
		}
	}
	return false;
}