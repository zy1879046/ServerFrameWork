//
// Created by 张羽 on 25-4-15.
//

#ifndef FIBER_H
#define FIBER_H
#include <functional>
#include <memory>
#include <ucontext.h>

namespace sylar
{
/*
 *
 * 实现非对称协程，一个线程有一个主协程，用来进行协程切换，子协程切换必须先切换到主协程在切换到子协程
 * Thread --->mainFiber <--> subFiber
 *              ^
 *              |
 *              v
 *           subFiber
 */

/*
 *协程实现基于 ucontext_t,
    *The ucontext_t structure contains at least these fields:
            ucontext_t *uc_link          context to assume when this one returns
            sigset_t uc_sigmask          signals being blocked
            stack_t uc_stack             stack area
            mcontext_t uc_mcontext       saved registers
    uc_link 当前协程执行完后执行的下一个协程
    uc_sigmask 信号掩码，表示需要屏蔽的信号
    uc_stack 协程运行的栈信息，uc_stack.ss_sp：栈指针指向stack uc_stack.ss_sp = stack;
                uc_stack.ss_size：栈大小 uc_stack.ss_size = stacksize;
    uc_mcontext 保存具体的程序执行上下文，如PC值，堆栈指针以及寄存器值等信息。
 *
 */


/*
 * 函数的使用
 * void makecontext(ucontext_t* ucp, void (*func)(), int argc, ...);
    * 初始化一个ucontext_t,func参数指明了该context的入口函数，argc为入口参数的个数，每个参数的类型必须是int类型。另外在makecontext前，
    * 一般需要显示的初始化栈信息以及信号掩码集同时也需要初始化uc_link，以便程序退出上下文后继续执行。
    int swapcontext(ucontext_t* olducp, ucontext_t* newucp);
    原子操作，该函数的工作是保存当前上下文并将上下文切换到新的上下文运行
    int getcontext(ucontext_t* ucp);
    将当前的执行上下文保存在cpu中，以便后续恢复上下文
    int setcontext(const ucontext_t* ucp)
    将当前程序切换到新的context,在执行正确的情况下该函数直接切换到新的执行状态，不会返回
 */
class Fiber : public std::enable_shared_from_this<Fiber>
{
public:
    using ptr = std::shared_ptr<Fiber>;
    //协程状态 分别表示 INIT：初始化 HOLD: 挂起 EXEC:执行 TERM:终止 READY:就绪 EXCEPT:异常
    enum State
    {
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        EXCEPT
    };
public:
    //子协程创建，传入回调函数，以及栈大小
    Fiber(std::function<void()> func,size_t stack_size = 0);
    ~Fiber();
    //切换到当前协程
    void swap_in();
    //切换到主协程
    void swap_out();
    //获取当前协程id
    uint64_t get_id();
    //获取协程状态
    State get_state();
    //重设回调函数
    void reset(std::function<void()> func);
public:
    //设置当前执行的协程
    static void SetCurFiber(Fiber* fiber);
    static Fiber::ptr GetCurFiber();
    //将当前正在执行的协程yied
    static void yeid_to_ready();
    static void yied_to_hold();
    static uint64_t get_total_fibers();
    static uint64_t get_cur_fiber_id();
private:
    //主协程创建，默认创建的，m_ctx中保存的是线程的当前上下文信息
    Fiber();
    static void MianFun();
private:
    uint64_t m_id = 0;//协程id
    uint32_t m_stacksize = 0;//协程栈的大小
    State m_state = State::INIT;//协程的状态
    ucontext_t m_ctx;//协程的上下文信息
    void* m_stack = nullptr;//协程栈

    std::function<void()> m_cb;//回调函数
};
}


#endif //FIBER_H
