//
// Created by 张羽 on 25-4-15.
//

#include "fiber.h"
#include "config.h"
#include "macro.h"
#include <atomic>
namespace sylar
{
    //定义默认栈大小
     auto g_stack_size = CONFIG_ENV()->lookup("fiber.stack_size",1024 * 1024,"fiber Stack Size");
    //线程当前的协程
    static thread_local Fiber* t_cur_fiber = nullptr;
    //线程的主协程
    static thread_local Fiber::ptr t_main_fiber = nullptr;
    //协程id
    static std::atomic<uint64_t> t_cur_fiber_id = 0;
    //协程数
    static std::atomic<uint64_t> t_fiber_count = 0;
    //自定义栈分配
    class MallocStackAllocator
    {
    public:
        static void* Alloc(size_t size){
            return malloc(size);
        }
        static void Dealloc(void* vp, size_t size){
            return free(vp);
        }
    };
    using StackAllocator = MallocStackAllocator;
    Fiber::Fiber(std::function<void()> func, size_t stack_size)
        :m_id(++t_cur_fiber_id),m_cb(func)
    {
        ++t_fiber_count;
        //读取当前栈大小
        m_stacksize = stack_size ? stack_size : g_stack_size->get_value();
        //创建栈空间
        m_stack = StackAllocator::Alloc(m_stacksize);
        m_state = INIT;
        if (getcontext(&m_ctx))
        {
            SYLAR_ASSERT2(false,"getcontext");
        }
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;
        m_ctx.uc_link = &t_main_fiber->m_ctx;
        makecontext(&m_ctx, (void(*)())MianFun, 0);
        LOG_SYSTEM_INFO() << "fiber id: " << m_id;
    }

    Fiber::~Fiber()
    {
        --t_fiber_count;
        //将栈释放
        if (m_stack != nullptr)
        {
            SYLAR_ASSERT(m_state == INIT || m_state == TERM);
            StackAllocator::Dealloc(m_stack, m_stacksize);
        }else//主协程
        {
            //主协程没有回调，且状态一直为EXEC
            SYLAR_ASSERT(!m_cb);
            SYLAR_ASSERT(m_state == EXEC);
            auto fiber = t_cur_fiber;
            if (fiber == this)
            {
                SetCurFiber(nullptr);
            }
        }
        LOG_ROOT_INFO() << "fiber id: " << m_id;
    }

    void Fiber::swap_in()
    {
        //设置当前执行协程
        SetCurFiber(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
        //从主协程切换到子协程
        if (swapcontext(&t_main_fiber->m_ctx,&m_ctx))
        {
            SYLAR_ASSERT2(false,"swapcontext");
        }
    }

    void Fiber::swap_out()
    {
        //从子协程切换到主协程
        SetCurFiber(t_main_fiber.get());
        if (swapcontext(&m_ctx,&t_main_fiber->m_ctx))
        {
            SYLAR_ASSERT2(false,"swapcontext");
        }
    }

    uint64_t Fiber::get_id()
    {
        return m_id;
    }

    Fiber::State Fiber::get_state()
    {
        return m_state;
    }

    void Fiber::reset(std::function<void()> func)
    {
        //必须有栈空间
        SYLAR_ASSERT(m_stack);
        //状态必须为非执行或执行过程中
        SYLAR_ASSERT(m_state == INIT || m_state == EXCEPT || m_state == TERM);
        m_cb = func;
        if (getcontext(&m_ctx))
        {
            SYLAR_ASSERT2(false,"getcontext");
        }
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;
        m_ctx.uc_link = &t_main_fiber->m_ctx;;
        makecontext(&m_ctx, (void(*)())MianFun, 0);
        m_state = INIT;
    }

    void Fiber::SetCurFiber(Fiber* fiber)
    {
        t_cur_fiber = fiber;
    }

    Fiber::ptr Fiber::GetCurFiber()
    {
        if(t_cur_fiber !=  nullptr)return t_cur_fiber->shared_from_this();
        //如果当前协程为nullptr，表示主协程还没有创建，创建主协程
        Fiber::ptr main_fiber(new Fiber());
        SYLAR_ASSERT(t_cur_fiber == main_fiber.get());
        t_main_fiber = main_fiber;
        return main_fiber->shared_from_this();
    }

    void Fiber::yeid_to_ready()
    {
        auto fiber = GetCurFiber();
        fiber->m_state = READY;
        fiber->swap_out();
    }

    void Fiber::yied_to_hold()
    {
        auto fiber = GetCurFiber();
        fiber->m_state = HOLD;
        fiber->swap_out();
    }

    uint64_t Fiber::get_total_fibers()
    {
        return t_fiber_count;
    }

    uint64_t Fiber::get_cur_fiber_id()
    {
        auto fiber = GetCurFiber();
        return fiber->m_id;
    }

    Fiber::Fiber()
    {
        m_state = EXEC;
        LOG_SYSTEM_INFO() << "current fiber id: " << t_cur_fiber_id;
        m_id = 0;
        SetCurFiber(this);
        ++t_fiber_count;
        //将当前上下文信息存到 m_ctx中
        if (getcontext(&m_ctx))
        {
            SYLAR_ASSERT2(false,"getcontext");
        }

    }

    void Fiber::MianFun()
    {
        auto cur = GetCurFiber();
        SYLAR_ASSERT(cur != nullptr);
        try
        {
            cur->m_cb();
            cur->m_cb = nullptr;
            cur->m_state = TERM;
        }catch(std::exception& ex){
            cur->m_state = EXCEPT;
            LOG_SYSTEM_ERROR()<< "Fiber Except" << ex.what();
        }catch(...){
            cur->m_state = EXCEPT;
            LOG_SYSTEM_ERROR() << "Fiber Except";
        }

        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr->swap_out();

        SYLAR_ASSERT2(false,"never reach");
    }
}

