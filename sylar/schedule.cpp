//
// Created by 张羽 on 25-4-15.
//

#include "schedule.h"
#include <cassert>
#include <memory>
#include "mutex.h"
#include "macro.h"
namespace sylar
{
    static thread_local Schedule* t_schedule = nullptr;
    static thread_local Fiber* t_schedule_fiber = nullptr;
    Schedule::Schedule(size_t thread_num, bool use_caller, std::string name)
        :m_name(std::move(name)),m_use_caller(use_caller)
    {
        SYLAR_ASSERT(thread_num > 0);
        //是否使用主线程作为调度协程的线程
        if (m_use_caller)
        {
            //创建主协程
            Fiber::GetCurFiber();
            --thread_num;
            SYLAR_ASSERT(t_schedule == nullptr);
            //主线程的调度器
            t_schedule = this;
            //主线程的调度协程
            // m_rootFiber = std::make_shared<Fiber>(std::bind(&Schedule::run,this));
            //将主线程的名字设置为调度器的名字
            Thread::set_name(m_name);
            //主线程的主协程
            t_schedule_fiber = Fiber::GetCurFiber().get();
            //获取主线程的id
            m_rootThread = get_thread_id();
            //将其放在id数组里
            m_threadIds.push_back(m_rootThread);
        }else
        {
            m_rootThread = -1;
        }
        m_threadCount = thread_num;
    }

    Schedule::~Schedule()
    {
        SYLAR_ASSERT(m_stopping);
        if (get_this() == this)
        {
            t_schedule = nullptr;
        }
    }

    Schedule* Schedule::get_this()
    {
        return t_schedule;
    }

    Fiber* Schedule::get_main_fiber()
    {
        return t_schedule_fiber;
    }

    void Schedule::start()
    {
        // LOG_ROOT_INFO() << "sylar::Schedule::start()";
        LockGuard<Mutex> lock(m_mutex);
        //false时表示启动了
        if (!m_stopping)
        {
            return;
        }
        m_stopping = false;
        SYLAR_ASSERT(m_threads.empty());
        //创建线程池
        m_threads.reserve(m_threadCount);
        for (size_t i = 0; i < m_threadCount; ++i)
        {
            m_threads.emplace_back(std::make_shared<Thread>([this] { run(); },m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->get_pid());
        }
        // if (m_rootFiber)m_rootFiber->swap_in();
        // LOG_ROOT_INFO() << "sylar::Schedule::start() end";
    }

    void Schedule::stop()
    {
        // LOG_ROOT_INFO() << "sylar::Schedule::stop()";
        m_autoStop = true;
        //当只有主线程调度时
        if (m_use_caller && m_threads.size() == 0)
        {
            // LOG_ROOT_INFO() << this << " stopped";
            m_stopping = true;
            if (stopping())
            {
                return;
            }
        }
        //use_caller的话,只有使用主线程作为协程调度时才设置了 t_schedule
        if (m_rootThread != -1)
        {
            SYLAR_ASSERT(get_this() == this);
        }else
        {
            SYLAR_ASSERT(get_this() != this);
        }
        m_stopping = true;
        //每个线程通知一次
        for (size_t i = 0; i <m_threadCount; ++i)
        {
            tickle();
        }
        if(m_use_caller)tickle();

        //主线程开始执行协程
        if(m_use_caller){
            if(!stopping()){
                // LOG_ROOT_INFO() << "m_rootFiber id " << m_rootFiber->get_id();
                // m_rootFiber->swap_in();
                run();
            }
        }
        std::vector<Thread::ptr> thr;
        {
            LockGuard<Mutex> lock(m_mutex);
            thr.swap(m_threads);
        }
        //等待所有线程执行完
        for (auto& i : thr)
        {
            i->join();
        }
    }

    void Schedule::tickle()
    {
        LOG_ROOT_INFO() << "sylar::Schedule::tickle()";
    }

    void Schedule::run()
    {
        // LOG_ROOT_INFO() << "sylar::Schedule::run()";
        //设置当前线程的调度器
        set_this();
        //如果不是主线程，设置其主协程
        if (sylar::get_thread_id() != m_rootThread)
        {
            //后面的部分也会创建主协程
            t_schedule_fiber = Fiber::GetCurFiber().get();
        }
        Fiber::ptr idle_fiber = std::make_shared<Fiber>(std::bind(&Schedule::idle,this));
        Fiber::ptr cb_fiber;
        Task task;//

        while (true)
        {
            task.reset();
            bool tickle_me = false;
            {
                LockGuard<Mutex> lock(m_mutex);
                auto it = m_tasks.begin();
                //从任务队列中取出任务
                while (it != m_tasks.end())
                {
                    //当前任务指定了线程执行，但是当前线程不是任务执行的线程
                    if (it->thread_id != -1 && sylar::get_thread_id() != it->thread_id)
                    {
                        ++it;
                        tickle_me = true;
                        continue;
                    }
                    SYLAR_ASSERT(it->fiber || it->cb);
                    //感觉没有什么用，任务队列中的任务一般没有执行态的
                    if (it->fiber && it->fiber->get_state() == Fiber::EXEC)
                    {
                        ++it;
                        continue;
                    }
                    task = *it;
                    m_tasks.erase(it);
                    ++m_activeThreadCount;
                    break;
                }
            }
            if (tickle_me)
            {
                tickle();
            }
            //执行当前取出的任务
            if (task.fiber && task.fiber->get_state() != Fiber::TERM && task.fiber->get_state()!=Fiber::EXCEPT)
            {
                task.fiber->swap_in();
                //执行完后
                --m_activeThreadCount;
                //根据当前协程状态判断是否加入到任务队列
                if (task.fiber->get_state() == Fiber::READY)
                {
                    schedule(task.fiber,task.thread_id);
                }else if (task.fiber->get_state() != Fiber::TERM && task.fiber->get_state()!=Fiber::EXCEPT)
                {
                    task.fiber->set_state(Fiber::HOLD);
                }
                task.reset();
            }else if (task.cb)
            {
                if (cb_fiber)
                {
                    cb_fiber->reset(task.cb);
                }else
                {
                    cb_fiber.reset(new Fiber(task.cb));
                    task.cb = nullptr;
                }
                cb_fiber->swap_in();
                --m_activeThreadCount;
                if (cb_fiber->get_state() == Fiber::READY)
                {
                    schedule(cb_fiber,task.thread_id);
                    cb_fiber.reset();
                }else if (cb_fiber->get_state() == Fiber::TERM || cb_fiber->get_state() == Fiber::EXCEPT)
                {
                    cb_fiber->reset(nullptr);
                }else
                {
                    cb_fiber->set_state(Fiber::HOLD);
                    cb_fiber.reset();
                }
                task.reset();
            }else
            {
                //线程退出条件
                if (idle_fiber->get_state() == Fiber::TERM)
                {
                    LOG_ROOT_INFO() << "idle fiber term";
                    break;
                }
                ++m_idleThreadCount;
                idle_fiber->swap_in();
                --m_idleThreadCount;
                if(idle_fiber->get_state() != Fiber::TERM && idle_fiber->get_state() != Fiber::EXCEPT){
                    idle_fiber->set_state(Fiber::HOLD);
                }
            }
        }
    }

    bool Schedule::stopping()
    {
        LockGuard<Mutex> lock(m_mutex);
        return m_stopping && m_autoStop && m_tasks.empty() && m_activeThreadCount == 0;
    }

    void Schedule::idle()
    {
        LOG_ROOT_INFO() << "sylar::Schedule::idle()";
        //调度器stop时才会将 State转换为TERM，这样run函数才会终止
        while (!stopping())
        {
            Fiber::yied_to_hold();
        }
    }

    void Schedule::set_this()
    {
        t_schedule = this;
    }

    bool Schedule::has_ilde_threads() const
    {
        return m_idleThreadCount > 0;
    }
}
