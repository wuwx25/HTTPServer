#ifndef LOCKER_H
#define LOCKER_H
#include<exception>
#include<pthread.h>
#include<semaphore.h>
/*封装信号量的类*/
class sem
{
public:
	/*创建并初始化信号量*/
	sem()
	{
		if (sem_init(&m_sem, 0, 0) != 0)
		{
			/*构造函数没有返回值，可以通过抛出异常来报告错误*/
			throw std::exception();
		}
	}
	/*销毁信号量*/ 
    ~sem()
	{
		sem_destroy(&m_sem);
	}
	/*等待信号量*/
	bool wait()
	{
		return sem_wait(&m_sem) == 0;
	}
	/*增加信号量*/
	bool post()
	{
		return sem_post(&m_sem) == 0;
	}
private:
	sem_t m_sem;
};
/*封装互斥锁的类*/
class locker
{
public:
	/*创建并初始化互斥锁*/
	locker()
	{
		if (pthread_mutex_init(&m_mutex, NULL) != 0)
		{
			throw std::exception();
		}
	}
	/*销毁互斥锁*/ 
    ~locker()
	{
		pthread_mutex_destroy(&m_mutex);
	}
	/*获取互斥锁*/
	bool lock()
	{
		return pthread_mutex_lock(&m_mutex) == 0;
	}
	/*释放互斥锁*/
	bool unlock()
	{
		return pthread_mutex_unlock(&m_mutex) == 0;
	}
private:
	pthread_mutex_t m_mutex;
};
#endif