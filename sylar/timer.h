//
// Created by 张羽 on 25-4-17.
//

#ifndef TIMER_H
#define TIMER_H
#include <functional>
#include <memory>
#include <set>

#include "mutex.h"

namespace sylar
{
class TimerManager;
/*
 *定时器的实现，时间到了会执行里面的回调函数
 */
class Timer : public std::enable_shared_from_this<Timer>
{
    friend class TimerManager;
public:
    using ptr = std::shared_ptr<Timer>;
    bool cancel();
    bool refresh();
    bool reset(uint64_t ms, bool from_now = true);

private:
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager);
    Timer(uint64_t next);
private:
    bool m_recurring{}; //是否重复
    uint64_t m_ms{}; //执行周期
    uint64_t m_next{}; //下次执行时间
    std::function<void()> m_cb;
    TimerManager *m_manager{};//定时器管理器

private:
    struct Comparator
    {
        bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
    };
};

class TimerManager
{
    friend class Timer;
public:
    TimerManager();
    virtual ~TimerManager();
    Timer::ptr add_timer(uint64_t ms, std::function<void()> cb,bool recurring = false);
    Timer::ptr add_condition_timer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false);
    uint64_t get_next_timer();
    void get_expired_cbs(std::vector<std::function<void()> > &cbs);
    bool has_timer();
protected:
    virtual void on_timer_insert_at_front() = 0;
    void add_timer(Timer::ptr timer, UniqueLock<Mutex>& lock);
private:
    //检测服务器时间是否被回拨
    bool detect_clock_roll_overflow(uint64_t now_ms);
private:
    Mutex m_mutex;
    std::set<Timer::ptr,Timer::Comparator> m_timers;
    bool m_tickled = false;
    uint64_t m_previousTime = 0;
};

}

#endif //TIMER_H
