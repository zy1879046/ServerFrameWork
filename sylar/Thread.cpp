//
// Created by 张羽 on 25-4-15.
//

#include "Thread.h"
#include "util.h"
namespace sylar
{
    static thread_local Thread* t_thread = nullptr;//记录当前线程
    static thread_local std::string t_name = "UNKNOW";//记录当前线程的名字
    Thread::~Thread()
    {
        if (m_pthread)
        {
            pthread_detach(m_pthread);
        }
    }

    void Thread::join()
    {
        if (m_is_joinable)
        {
            pthread_join(m_pthread, nullptr);
            m_is_joinable = false;
        }
    }
    Thread* Thread::GetThis()
    {
        //获取当前线程
        return t_thread;
    }
    const std::string Thread::get_name()
    {
        return m_name;
    }

    void Thread::set_name(const std::string name)
    {
        m_name = name;
        t_name = name;
    }

    const pid_t Thread::get_pid()
    {
        return m_pid;
    }

    void* Thread::thread_func(void* arg)
    {
        auto self = static_cast<Thread*>(arg);
        t_thread = self;
        t_name = self->get_name();
        //设置pid
        self->m_pid = get_thread_id();
        //设置线程名字
        pthread_setname_np(self->m_pthread,self->m_name.substr(0,15).c_str());
        self->m_sem.notify();
        ThreadFunc cb;
        cb.swap(self->m_func);
        cb();
        return nullptr;
    }
}

