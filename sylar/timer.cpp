//
// Created by 张羽 on 25-4-17.
//
#include "timer.h"

#include <utility>
#include "util.h"
namespace sylar
{
    bool Timer::cancel()
    {
        LockGuard lock(m_manager->m_mutex);
        if (m_cb)
        {
            m_cb = nullptr;
            auto it = m_manager->m_timers.find(shared_from_this());
            m_manager->m_timers.erase(it);
            return true;
        }
        return false;
    }

    bool Timer::refresh()
    {
        LockGuard lock(m_manager->m_mutex);
        if (!m_cb)return false;
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end())
        {
            return false;
        }
        m_manager->m_timers.erase(it);
        m_next = sylar::get_cur_ms() + m_ms;
        m_manager->m_timers.insert(shared_from_this());
        return true;
    }

    bool Timer::reset(uint64_t ms, bool from_now)
    {
        if (ms == m_ms && !from_now)
        {
            return true;
        }
        UniqueLock lock{m_manager->m_mutex};
        if (!m_cb)return false;
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end())
        {
            return false;
        }
        m_manager->m_timers.erase(it);
        uint64_t start = 0;
        if (from_now)
        {
            start = sylar::get_cur_ms();
        }
        else
        {
            start = m_next - m_ms;
        }
        m_ms = ms;
        m_next = start + ms;
        m_manager->add_timer(shared_from_this(), lock);
        return true;
    }

    Timer::Timer(const uint64_t ms, std::function<void()> cb, const bool recurring, TimerManager* manager)
        :m_recurring(recurring),m_ms(ms),m_next(sylar::get_cur_ms() + ms),m_cb(std::move(cb)),m_manager(manager)
    {
    }

    Timer::Timer(const uint64_t next)
        :m_next(next)
    {
    }

    bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const
    {
        if (!lhs && !rhs)return false;
        if (!lhs)return true;
        if (!rhs)return false;
        return lhs->m_next < rhs->m_next;
    }

    TimerManager::TimerManager()
        :m_previousTime(sylar::get_cur_ms())
    {
    }

    TimerManager::~TimerManager()
    {
    }

    Timer::ptr TimerManager::add_timer(uint64_t ms, std::function<void()> cb, bool recurring)
    {
        Timer::ptr timer( new Timer(ms, std::move(cb), recurring, this));
        UniqueLock lock(m_mutex);
        add_timer(timer,lock);
        return timer;
    }
    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb)
    {
        auto con = weak_cond.lock();
        if (con)
        {
            cb();
        }
    }
    Timer::ptr TimerManager::add_condition_timer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond,
        bool recurring)
    {
        return add_timer(ms,[weak_cond, cb] { OnTimer(weak_cond, cb); },recurring);
    }

    uint64_t TimerManager::get_next_timer()
    {
        LockGuard lock(m_mutex);
        if (m_timers.empty())
        {
            return ~0ull;
        }
        auto it = *m_timers.begin();
        uint64_t now = sylar::get_cur_ms();
        if (now >= it->m_next)
        {
            return 0;
        }else
        {
            return it->m_next - now;
        }
    }

    void TimerManager::get_expired_cbs(std::vector<std::function<void()>>& cbs)
    {
        // 获取当前时间
        uint64_t now = sylar::get_cur_ms();
        std::vector<Timer::ptr> timers;//存储过期的定时器
        LockGuard lock(m_mutex);
        if (m_timers.empty())
        {
            return;
        }
        bool roll_over = detect_clock_roll_overflow(now);
        //没有回波且定时器的下一个时间大于当前时间
        if (!roll_over && (*m_timers.begin())->m_next > now)
        {
            return;
        }
        Timer::ptr now_timer(new Timer(now));
        //回滚则全部取出，没有则取出小的
        auto it = roll_over ? m_timers.end() : m_timers.lower_bound(now_timer);
        //取出等于的
        while (it != m_timers.end() && (*it)->m_next == now)
        {
            ++it;
        }
        //将过期的定时器放入到timers中
        timers.insert(timers.begin(), m_timers.begin(), it);
        m_timers.erase(m_timers.begin(), it);
        //将过期的定时器的回调函数放入到cbs中
        cbs.reserve(timers.size());
        for (auto& timer : timers)
        {
            cbs.push_back(timer->m_cb);
            if (timer->m_recurring)
            {
                timer->m_next = now + timer->m_ms;
                m_timers.insert(timer);
            }else
            {
                timer->m_cb = nullptr;
            }
        }
    }

    bool TimerManager::has_timer()
    {
        LockGuard lock(m_mutex);
        return !m_timers.empty();
    }

    void TimerManager::add_timer(Timer::ptr timer, UniqueLock<Mutex>& lock)
    {
        auto it = m_timers.insert(timer).first;
        bool at_front = (it == m_timers.begin()) && !m_tickled;
        if (at_front)
        {
            m_tickled = true;
            lock.unlock();
            on_timer_insert_at_front();
        }
    }

    bool TimerManager::detect_clock_roll_overflow(uint64_t now_ms)
    {
        bool rollover = false;
        if(now_ms < m_previousTime &&
                now_ms < (m_previousTime - 60 * 60 * 1000)) {
            rollover = true;
                }
        m_previousTime = now_ms;
        return rollover;
    }
}
