//
// Created by 张羽 on 25-4-17.
//

#ifndef IOMANAGER_H
#define IOMANAGER_H


#include "schedule.h"
#include "timer.h"
/**
 * IOManager 主要是在Schedule的基础上增加了IO事件的处理,也就是在idle函数中处理IO事件
 *
 **/
namespace sylar
{
    /*
     * 基于epoll的协程IO调度器
     */
    class IOManager : public Schedule,
    public TimerManager
    {
    public:
        using ptr = std::shared_ptr<IOManager>;
        enum Event
        {
            None = 0x0,
            Read = 0x1,
            Write = 0x4,
        };
    private:
        /**
         * socket事件的结构体
         */
        struct FdContext
        {
            using ptr = std::shared_ptr<FdContext>;
            /**
             * 事件上下文
             */
            struct EventContext
            {
                Schedule* schedule = nullptr;
                Fiber::ptr fiber;
                std::function<void()> cb;
            };
            //获取事件上下文
            EventContext& getContext(Event event);
            //重置事件上下文
            void resetContext(EventContext& ctx);
            //触发事件，触发事件会将事件从 events中删除，同时相关的事件会被重置
            void triggerEvent(Event event);

            EventContext read;//读事件
            EventContext write;//写事件
            int fd = 0;//文件描述符
            Event events = None;//注册的事件
            Mutex mutex;
        };
    public:
        IOManager(size_t thread_num = 1, bool use_caller = true, const std::string& name = "");
        ~IOManager() override;
        int add_event(int fd, Event event, std::function<void()> cb = nullptr);
        //删除事件，事件存在不会触发
        bool del_event(int fd, Event event);
        //取消事件，事件存在会触发
        bool cancel_event(int fd, Event event);
        //取消所有事件，事件存在会触发
        bool cancel_all(int fd);

        static IOManager* get_this();
    protected:
        void tickle() override;
        bool stopping() override;
        void idle() override;
        void on_timer_insert_at_front() override;
        void context_resize(size_t size);
        bool stopping(uint64_t& timeout);

    private:
        int m_epfd = 0;//epoll的文件描述符
        int m_tickleFds[2]{};//唤醒epoll的文件描述符 使用pipe实现
        std::atomic<size_t> m_pendingEventCount = {0};//挂起的事件数
        Mutex m_mutex;
        std::vector<FdContext::ptr> m_fdContexts;
    };
}
#endif //IOMANAGER_H
