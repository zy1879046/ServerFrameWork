//
// Created by 张羽 on 25-4-15.
//

#ifndef THREAD_H
#define THREAD_H
#include <functional>
#include <thread>
#include <memory>
#include <pthread.h>
#include "mutex.h"
#include <string>

#include "log.h"

namespace sylar{
class Thread final : NonCopyable
{
public:
    using ThreadFunc = std::function<void()>;
    using ptr = std::shared_ptr<Thread>;
    template<typename Fun,typename... Args>
    explicit Thread(Fun&& func,Args&&... args, const std::string name = "")
        :m_name(name),m_is_joinable(true)
    {
        if (name.empty())
        {
            m_name = "UNKNOW";
        }
        m_func = std::bind(func, std::forward<Args>(args)...);
        int rt = pthread_create(&m_pthread,nullptr,&Thread::thread_func,this);
        if(rt){
            LOG_ROOT_ERROR() << "pthread_create thread fail, rt = " << rt << " nama = " <<name;
            throw std::logic_error("pthread_create error");
        }
        m_sem.wait();
    }
    ~Thread()override;
    void join();
    const std::string get_name();
    void set_name(const std::string name);
    const pid_t get_pid();
    static Thread* GetThis();
private:
    static void* thread_func(void* arg);

    pid_t m_pid;
    pthread_t m_pthread;
    std::string m_name;
    ThreadFunc m_func;
    Semaphore m_sem;
    bool m_is_joinable;
};


}


#endif //THREAD_H
