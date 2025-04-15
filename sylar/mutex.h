//
// Created by 张羽 on 25-4-14.
//

#ifndef MUTEX_H
#define MUTEX_H
#include <semaphore.h>
#include <pthread.h>

#include "noncopyable.h"
namespace sylar
{
    //Mutex封装
    class Mutex final :NonCopyable
    {
    public:
        Mutex()
        {
            pthread_mutex_init(&m_mutex, nullptr);
        }
        ~Mutex() override
        {
            pthread_mutex_destroy(&m_mutex);
        }

        void lock()
        {
            pthread_mutex_lock(&m_mutex);
        }

        void unlock()
        {
            pthread_mutex_unlock(&m_mutex);
        }
        pthread_mutex_t* get(){return &m_mutex;}
    private:
        pthread_mutex_t m_mutex;
    };
    //信号量封装
    class Semaphore final :NonCopyable
    {
    public:
        explicit Semaphore(unsigned int count = 0)
        {
            sem_init(&m_sem, 0, count);
        }
        ~Semaphore() override
        {
            sem_destroy(&m_sem);
        }
        void wait()
        {
            sem_wait(&m_sem);
        }
        void notify()
        {
            sem_post(&m_sem);
        }
    private:
        sem_t m_sem;
    };

    template <typename T>
    class LockGuard final : NonCopyable
    {
    public:
        explicit LockGuard(T& mutex):m_mutex(mutex)
        {
            mutex.lock();
        }
        ~LockGuard() override
        {
            m_mutex.unlock();
        }
    private:
        T& m_mutex;
    };

    template <typename T>
    class UniqueLock final : NonCopyable
    {
    public:
        explicit UniqueLock(T& mutex):m_mutex(mutex)
        {
            m_mutex.lock();
            m_isLocked = true;
        }
        ~UniqueLock() override
        {
            if (m_isLocked)
            {
                m_mutex.unlock();
            }
        }
        void lock()
        {
            m_mutex.lock();
        }
        void unlock()
        {
            m_mutex.unlock();
        }

        T& mutex(){return m_mutex;}
    private:
        T& m_mutex;
        bool m_isLocked = false;
    };

    class ConditionVariable final : NonCopyable
    {
    public:
        ConditionVariable()
        {
            pthread_cond_init(&m_cond, nullptr);
        }

        ~ConditionVariable()override
        {
            pthread_cond_destroy(&m_cond);
        }
        void wait(UniqueLock<Mutex>&lock)
        {
            pthread_cond_wait(&m_cond,lock.mutex().get());
        }
        template <typename Fun>
        void wait(UniqueLock<Mutex>& lock,Fun&& f)
        {
            while (!f())
            {
                wait(lock);
            }
        }
        void notify()
        {
            pthread_cond_signal(&m_cond);
        }
        void notifyAll()
        {
            pthread_cond_broadcast(&m_cond);
        }
    private:
        pthread_cond_t m_cond;
    };
}

#endif //MUTEX_H
