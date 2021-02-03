#include "threadpool.h"

threadpool::threadpool(int thread_number, int max_requests) :
	m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL)
{
	if ((thread_number <= 0) || (max_requests <= 0))
	{
		throw std::exception();
	}
	m_threads = new pthread_t[m_thread_number];
	if (!m_threads)
	{
		throw std::exception();
	}
	/*创建thread_number个线程，并将它们都设置为脱离线程*/
	for (int i = 0; i<thread_number; ++i)
	{
		printf("create the%dth thread\n", i);
		if (pthread_create(m_threads + i, NULL, worker, this) != 0)
		{
			delete[]m_threads;
			throw std::exception();
		}
		if (pthread_detach(m_threads[i]))
		{
			delete[]m_threads;
			throw std::exception();
		}
	}
}

threadpool::~threadpool()
{
	delete[]m_threads;
	m_stop = true;
}

bool threadpool::append(http_conn* request)
{
	/*操作工作队列时一定要加锁，因为它被所有线程共享*/
	m_queuelocker.lock();
	if (m_workqueue.size()>m_max_requests)
	{
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();
	return true;
}

/*pthread_create的第三个参数必须是static函数，但static函数无法对类内的成员进行访问，
因此将this指针作为参数传入worker中*/
void* threadpool::worker(void*arg)
{
	threadpool*pool = (threadpool*)arg;
	pool ->run();
	return pool;
}

void threadpool::run()
{
	while (!m_stop)
	{
		m_queuestat.wait();
		m_queuelocker.lock();
		if (m_workqueue.empty())
		{
			m_queuelocker.unlock();
			continue;
		}
		http_conn* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelocker.unlock();
		if (!request)
		{
			continue;
		}
		request ->process();
	}
}