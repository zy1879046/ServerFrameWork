//
// Created by 张羽 on 25-4-15.
//

#ifndef SCHEDULE_H
#define SCHEDULE_H
#include <vector>
#include "Thread.h"
#include "fiber.h"
#include <functional>
#include <list>

namespace sylar
{
/*
 * 协程调度器 N-M格式，N个线程 M个协程，本质是线程池里调度的是协程任务
 */
class Schedule {
public:
    using ptr = std::shared_ptr<Schedule>;
    explicit Schedule(size_t thread_num = 1,bool use_caller = true,std::string name = "");
    ~Schedule();
    //返回当前线程的调度器
    static Schedule* get_this();
    //当前主线程也作为调度协程的线程的话，该函数获取主线程的主协程
    static Fiber* get_main_fiber();
    void start();
    void stop();
public:
    template<typename FiberOrCb>
    void schedule(FiberOrCb&& fb,int thread = -1)
    {
        bool need_tickle = false;
        {
            LockGuard lock(m_mutex);
            need_tickle = scheduleNoLock(fb,thread);
        }
        if (need_tickle)
        {
            tickle();
        }
    }
    template<typename InputIterator>
    void schedule(InputIterator&& begin,InputIterator&& end ,int thread = -1)
    {
        bool need_tickle = false;
        {
            LockGuard lock(m_mutex);
            while(begin != end){
                need_tickle = scheduleNoLock(&(*begin),thread) || need_tickle;
                ++begin;
            }
        }
        if(need_tickle){
            tickle();
        }
    }
protected:
    virtual void tickle();
    virtual void run();
    virtual bool stopping();
    virtual void idle();
    void set_this();
    bool has_ilde_threads() const;
private:
    //无锁加入调度
    template <typename FiberOrCb>
    bool scheduleNoLock(FiberOrCb&& fb,int thread)
    {
        bool need_tickle = m_tasks.empty();
        Task task(fb,thread);
        if (task.cb != nullptr || task.fiber != nullptr)
        {
            m_tasks.push_back(task);
        }
        return need_tickle;
    }
    //调度任务
    struct Task
    {
        Task() : thread_id(-1){}
        Task(Fiber::ptr fb,int thread = -1):fiber(fb),thread_id(thread){}
        Task(Fiber::ptr* fb,int thread = -1):thread_id(thread){fiber.swap(*fb);}
        Task(std::function<void()> func,int thread = -1) : cb(func),thread_id(thread){}
        Task(std::function<void()>* func,int thread = -1) :thread_id(thread){cb.swap(*func);}
        template<typename Fun,typename... Args>
        Task(Fun&& func,Args... args,int thread = -1):cb(std::bind(func,std::forward<Args>(args)...)),thread_id(thread){}
        void reset()
        {
            fiber.reset();
            cb = nullptr;
            thread_id = -1;
        }
        Fiber::ptr fiber;//协程
        std::function<void()> cb; //回调
        int thread_id;//是否指定线程
    };
private:
    std::vector<Thread::ptr> m_threads;//线程池
    Mutex m_mutex;
    std::string m_name;//调度器名字
    std::list<Task> m_tasks;//任务队列
    // Fiber::ptr m_rootFiber;//主协程
    bool m_use_caller;
protected:
    std::vector<int> m_threadIds;// 线程的id数组
    size_t m_threadCount = 0;//线程的数量
    std::atomic<size_t> m_activeThreadCount = {0};//工作的线程
    std::atomic<size_t> m_idleThreadCount = {0};//空闲的线程
    bool m_stopping = true;//是否正在停止
    bool m_autoStop = false;//
    int m_rootThread = 0;//主线程的id(use_caller)
};
}


#endif //SCHEDULE_H
