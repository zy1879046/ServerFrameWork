//
// Created by 张羽 on 25-4-17.
//
#include "iomanager.h"

#include <cassert>

#include "macro.h"
#include <sys/epoll.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <cmath>
#include "hook.h"
namespace  sylar
{
    IOManager::FdContext::EventContext& IOManager::FdContext::getContext(Event event)
    {
        switch(event)
        {
            case Event::Read:
                return read;
            case Event::Write:
                return write;
            default:
                SYLAR_ASSERT2(false,"getContext");
        }
        throw std::invalid_argument("getContext invalid event");
    }

    void IOManager::FdContext::resetContext(EventContext& ctx)
    {
        ctx.cb = nullptr;
        ctx.fiber = nullptr;
        ctx.schedule = nullptr;
    }

    void IOManager::FdContext::triggerEvent(Event event)
    {
        SYLAR_ASSERT(event & events)
        events = Event(events & ~event);//清除事件
        EventContext& ctx = getContext(event);
        if (ctx.cb)
        {
            ctx.schedule->schedule(&ctx.cb);
        }else
        {
            ctx.schedule->schedule(&ctx.fiber);
        }
        ctx.schedule = nullptr;
    }

    IOManager::IOManager(size_t thread_num, bool use_caller, const std::string& name)
        :Schedule(thread_num, use_caller, name)
    {
        // epoll_create 创建成功后返回一个文件描述符，失败返回-1
        m_epfd = epoll_create(5000);
        SYLAR_ASSERT(m_epfd > 0);
        //pipe创建管道，成功返回0，失败返回-1
        int rt = pipe(m_tickleFds);
        SYLAR_ASSERT(rt == 0);

        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = m_tickleFds[0];
        rt = fcntl(m_tickleFds[0],F_SETFL,O_NONBLOCK);//设置非阻塞
        SYLAR_ASSERT(rt != -1);
        rt = epoll_ctl(m_epfd,EPOLL_CTL_ADD,m_tickleFds[0],&ev);//将读端加入到epoll中
        SYLAR_ASSERT(rt != -1);
        context_resize(32);
        //启动调度器
        start();
    }

    IOManager::~IOManager()
    {
        //停止调度器
        stop();
        close(m_epfd);
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);
    }

    int IOManager::add_event(int fd, Event event, std::function<void()> cb)
    {
        FdContext::ptr fd_ctx;
        UniqueLock lock(m_mutex);
        //当前fd上下文在fd上下文数组中
        if (static_cast<int>(m_fdContexts.size()) > fd)
        {
            fd_ctx = m_fdContexts[fd];
            lock.unlock();
        }else
        {
            lock.unlock();
            context_resize(fd * 1.5);
            fd_ctx = m_fdContexts[fd];
        }
        LockGuard lock2(fd_ctx->mutex);
        //下面对fd_ctx进行操作
        //不能添加相同的事件
        if (fd_ctx->events & event)
        {
            LOG_ROOT_ERROR() << "add_event assert fd=" << fd
                << " event=" << static_cast<int>(event)
                << " fd.events=" << static_cast<int>(fd_ctx->events);
            SYLAR_ASSERT2(false,"add_event assert");
        }
        //根据当前时间是否为None表示添加还是修改
        int op = fd_ctx->events == Event::None ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
        epoll_event ev;
        ev.events = EPOLLET | fd_ctx->events | event;
        ev.data.fd = fd;
        //添加事件
        int rt = epoll_ctl(m_epfd, op, fd, &ev);
        if (rt)
        {
            LOG_ROOT_ERROR() << "epoll_ctl(" << m_epfd << ", " << op
                << ", " << fd << "):" << rt << " (" << errno
                << ") (" << strerror(errno) << ")";
            return -1;
        }
        ++m_pendingEventCount;
        fd_ctx->events = Event(fd_ctx->events | event);
        //获取该事件上下文
        FdContext::EventContext& ctx = fd_ctx->getContext(event);
        SYLAR_ASSERT(!ctx.cb && !ctx.fiber && !ctx.schedule);
        //设置事件上下文
        ctx.schedule = Schedule::get_this();
        //如果有回调函数，执行回调函数，否则设置协程为当前协程
        if (cb)ctx.cb.swap(cb);
        else
        {
            ctx.fiber = Fiber::GetCurFiber();
            SYLAR_ASSERT2(ctx.fiber->get_state() == Fiber::EXEC,"state = " << ctx.fiber->get_state());
        }
        return 0;
    }

    bool IOManager::del_event(int fd, Event event)
    {
        UniqueLock lock(m_mutex);
        //不存在该fd
        if (static_cast<int>(m_fdContexts.size()) <= fd)
        {
            return false;
        }
        auto fd_ctx = m_fdContexts[fd];
        lock.unlock();
        //对该上下文进行操作
        LockGuard lock2(fd_ctx->mutex);
        //不存在该事件
        if (!(fd_ctx->events & event))
        {
            return false;
        }
        Event new_event = Event(fd_ctx->events & ~event);
        //还有事件就修改，没有就删除
        int op = new_event == Event::None ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
        epoll_event ev;
        ev.events = EPOLLET | new_event;
        ev.data.fd = fd;
        //删除事件
        int rt = epoll_ctl(m_epfd, op, fd, &ev);
        if (rt)
        {
            LOG_ROOT_ERROR() << "epoll_ctl(" << m_epfd << ", " << op
                << ", " << fd << "):" << rt << " (" << errno
                << ") (" << strerror(errno) << ")";
            return false;
        }
        --m_pendingEventCount;
        fd_ctx->events = new_event;
        //获取该事件上下文
        FdContext::EventContext& ctx = fd_ctx->getContext(event);
        //重置事件上下文
        fd_ctx->resetContext(ctx);
        return true;
    }

    bool IOManager::cancel_event(int fd, Event event)
    {
        UniqueLock lock(m_mutex);
        //不存在该fd
        if (static_cast<int>(m_fdContexts.size()) <= fd)
        {
            return false;
        }
        auto fd_ctx = m_fdContexts[fd];
        lock.unlock();
        //对该上下文进行操作
        LockGuard lock2(fd_ctx->mutex);
        //不存在该事件
        if (!(fd_ctx->events & event))
        {
            return false;
        }
        Event new_event = Event(fd_ctx->events & ~event);
        //还有事件就修改，没有就删除
        int op = new_event == Event::None ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
        epoll_event ev;
        ev.events = EPOLLET | new_event;
        ev.data.fd = fd;
        //删除事件
        int rt = epoll_ctl(m_epfd, op, fd, &ev);
        if (rt)
        {
            LOG_ROOT_ERROR() << "epoll_ctl(" << m_epfd << ", " << op
                << ", " << fd << "):" << rt << " (" << errno
                << ") (" << strerror(errno) << ")";
            return false;
        }
        --m_pendingEventCount;
        fd_ctx->triggerEvent(event);
        return true;
    }

    bool IOManager::cancel_all(int fd)
    {
        UniqueLock lock(m_mutex);
        //不存在该fd
        if (static_cast<int>(m_fdContexts.size()) <= fd)
        {
            return false;
        }
        auto fd_ctx = m_fdContexts[fd];
        lock.unlock();
        //对该上下文进行操作
        LockGuard lock2(fd_ctx->mutex);
        //不存在该事件
        if (fd_ctx->events == Event::None)
        {
            return false;
        }
        int op = EPOLL_CTL_DEL;
        epoll_event ev;
        ev.events = 0;
        ev.data.fd = fd;
        //删除事件
        int rt = epoll_ctl(m_epfd, op, fd, &ev);
        if (rt)
        {
            LOG_ROOT_ERROR() << "epoll_ctl(" << m_epfd << ", " << op
                << ", " << fd << "):" << rt << " (" << errno
                << ") (" << strerror(errno) << ")";
            return false;
        }
        if (fd_ctx->events & Read)
        {
            fd_ctx->triggerEvent(Event::Read);
            --m_pendingEventCount;
        }
        if (fd_ctx->events & Write)
        {
            fd_ctx->triggerEvent(Event::Write);
            --m_pendingEventCount;
        }
        SYLAR_ASSERT(fd_ctx->events == 0);
        return true;
    }

    IOManager* IOManager::get_this()
    {
        return dynamic_cast<IOManager*>(Schedule::get_this());
    }

    void IOManager::tickle()
    {
        if (!has_ilde_threads())
        {
            return;
        }
        //唤醒空闲协程
        int rt = write(m_tickleFds[1], "T", 1);
        SYLAR_ASSERT(rt == 1);
    }

    bool IOManager::stopping()
    {
        uint64_t timeout = 0;
        return stopping(timeout);
    }

    void IOManager::idle()
    {
        // LOG_ROOT_INFO() << "IOManager::idle()";
        const uint64_t MAX_EVENTS = 256;
        epoll_event* events = new epoll_event[MAX_EVENTS];
        std::shared_ptr<epoll_event> evts_ptr(events, [](epoll_event* ptr) {
            delete[] ptr;
        });
        while (true)
        {
            uint64_t timeout = 0;
            if (stopping(timeout))
            {
                LOG_ROOT_INFO() << "IOManager::idle() stopping";
                break;
            }
            int rt = 0;
            do
            {
                static const int MAX_TIMEOUT = 3000;
                timeout = std::min(timeout, static_cast<uint64_t>(MAX_TIMEOUT));
                rt = epoll_wait(m_epfd, events, MAX_EVENTS, timeout);
                //被中断了则重新wait
                if (rt < 0 && errno == EINTR)
                {
                    continue;
                }else
                {
                    break;
                }

            }while (true);
            std::vector<std::function<void()>> cbs;
            //获取所有过期的定时器回调,并将其加入到调度队列中
            get_expired_cbs(cbs);
            if (!cbs.empty())
            {
                schedule(cbs.begin(), cbs.end());
                cbs.clear();
            }
            for (int i =0 ; i < rt; ++i)
            {
                epoll_event& ev = events[i];
                if (ev.data.fd == m_tickleFds[0])
                {
                    uint8_t dummy[256];
                    //读取管道数据
                    while (read(m_tickleFds[0], dummy, sizeof(dummy)) > 0);
                    continue;
                }
                auto fd_ctx = m_fdContexts[ev.data.fd];
                //处理上下文事件
                LockGuard lock(fd_ctx->mutex);
                // 如果遇到错误或者挂起事件，则直接将事件全部触发
                if (ev.events & (EPOLLERR | EPOLLHUP))
                {
                    ev.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
                }
                int real_ev = None;
                if (ev.events & EPOLLIN)
                {
                    real_ev |= Read;
                }
                if (ev.events & EPOLLOUT)
                {
                    real_ev |= Write;
                }
                if ((real_ev & fd_ctx->events) == None)
                {
                    continue;
                }
                //剩余的事件
                int left_events = fd_ctx->events & ~real_ev;
                //如果没有剩余事件，则删除
                int op = left_events == None ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
                ev.events = left_events | EPOLLET;
                int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &ev);
                if (rt2)
                {
                    LOG_ROOT_ERROR() << "epoll_ctl(" << m_epfd << ", " << op<<
                        ", " << fd_ctx->fd << "):" << rt2 << " (" << errno
                        << ") (" << strerror(errno) << ")";
                    continue;
                }
                if (real_ev & Read)
                {
                    fd_ctx->triggerEvent(Event::Read);
                    --m_pendingEventCount;
                }
                if (real_ev & Write)
                {
                    fd_ctx->triggerEvent(Event::Write);
                    --m_pendingEventCount;
                }

            }
            Fiber::ptr cur = Fiber::GetCurFiber();
            auto raw_ptr = cur.get();
            cur.reset();
            raw_ptr->swap_out();
        }

    }

    void IOManager::on_timer_insert_at_front()
    {
        //在前面则唤醒空闲协程
        tickle();
    }

    void IOManager::context_resize(size_t size)
    {
        m_fdContexts.resize(size);
        for (size_t i = 0; i < size; ++i)
        {
            if (!m_fdContexts[i])
            {
                m_fdContexts[i] = std::make_shared<FdContext>();
                m_fdContexts[i]->fd = i;
            }
        }
    }

    bool IOManager::stopping(uint64_t& timeout)
    {
        timeout = get_next_timer();
        return timeout == ~0ull && m_pendingEventCount == 0 && Schedule::stopping();
    }

    void IOManager::run()
    {
        set_hook_enable(true);
        Schedule::run();
    }
}
