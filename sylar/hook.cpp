//
// Created by 张羽 on 25-4-19.
//

#include "hook.h"
#include "config.h"
#include <dlfcn.h>
#include "fd_manager.h"
#include "iomanager.h"
#include <stdarg.h>
#include <fcntl.h>

namespace sylar
{
    static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout = CONFIG_ENV()->lookup("tcp.connect.timeout", 5000, "tcp connect timeout");
    static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep)        \
    XX(usleep)       \
    XX(nanosleep)    \
    XX(socket)        \
    XX(connect)       \
    XX(accept)        \
    XX(read)          \
    XX(readv)         \
    XX(recv)          \
    XX(recvfrom)      \
    XX(recvmsg)       \
    XX(write)         \
    XX(writev)        \
    XX(send)          \
    XX(sendto)        \
    XX(sendmsg)       \
    XX(close)         \
    XX(ioctl)         \
    XX(fcntl)         \
    XX(getsockopt)    \
    XX(setsockopt)

    bool hook_init()
    {
        static bool hook_inited = false;
        if (hook_inited)
        {
            return true;
        }

        // void *dlsym(void *handle, const char *symbol);
        //该函数的作用是获取动态链接库中的函数地址 handle是动态链接库的句柄，symbol是函数名，返回函数地址，然后将该地址赋值给函数指针，
        //这样就可以避免直接调用系统函数了，同时没有办法调用系统调用函数

#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
        HOOK_FUN(XX);
#undef XX
        hook_inited = true;
        return true;
    }
    static uint64_t s_connect_timeout = -1;
    struct HookIniter
    {
        HookIniter()
        {
            hook_init();
            s_connect_timeout = g_tcp_connect_timeout->get_value();
            g_tcp_connect_timeout->add_listener([](const int& old_value, const int& new_value) {
                s_connect_timeout = new_value;
            });
        }
    };
    static HookIniter s_hook_initer;
    bool is_hook_enable()
    {
        return t_hook_enable;
    }
    void set_hook_enable(bool enable)
    {
        t_hook_enable = enable;
    }

    struct timer_info//条件定时器条件
    {
        int canceled = 0;
    };
    template<typename OriginalFun, typename... Args>
    static ssize_t do_io(int fd,OriginalFun fun,const char* hook_fun_name,uint32_t event,int timeout_so,
        Args... args)
    {
        //没有hook就直接调用
        if (sylar::t_hook_enable)
        {
            return fun(fd, std::forward<Args>(args)...);
        }
        //如果当前文件描述符关闭了
        auto ctx = sylar::FdManager::GetInstance()->get_fd(fd);
        //如果当前文件描述符不是被管理的，直接调用原始系统调用
        if (!ctx)
        {
            return fun(fd, std::forward<Args>(args)...);
        }
        //当前句柄关闭了
        if (ctx->is_closed())
        {
            errno = EBADF;//文件描述符错误
            return -1;
        }
        //如果不是socket或者是用户设置的非阻塞，直接调用系统调用
        if (!ctx->is_socket() || ctx->get_user_block())
        {
            return fun(fd, std::forward<Args>(args)...);
        }
        //获取当前设置的超时时间
        uint64_t to = ctx->get_timeout(timeout_so);
        std::shared_ptr<timer_info> tinfo = std::make_shared<timer_info>();
        ssize_t ret = fun(fd, std::forward<Args>(args)...);
        //如果是由于系统中断导致的停止，重新执行
        while (true){
            while (ret == -1 && errno == EINTR)
            {
                ret = fun(fd, std::forward<Args>(args)...);
            }
            //如果是由于没有可处理的数据导致的
            if (ret == -1 && errno == EAGAIN)
            {
                IOManager* iom = IOManager::get_this();
                Timer::ptr timer;
                std::weak_ptr<timer_info> winfo(tinfo);
                timer = iom->add_condition_timer(to,[winfo,fd,iom,event]
                {
                    auto t = winfo.lock();
                    //如果条件不满足或者取消了
                    if (!t || t->canceled)
                    {
                        return;
                    }
                    t->canceled = ETIMEDOUT;//否则说明超时了
                    //超时就取消该事件
                    iom->cancel_event(fd,static_cast<IOManager::Event>(event));
                },winfo);
                //不然就添加该事件到iom中去
                int rt = iom->add_event(fd,static_cast<IOManager::Event>(event));
                //如果添加失败
                if (rt)
                {
                    LOG_ROOT_ERROR() << hook_fun_name << " add_event(" << fd << ", "
                        << static_cast<int>(event) << ") error";
                    if (timer)
                    {
                        timer->cancel();
                    }
                    return -1;
                }else
                {
                    Fiber::yied_to_hold();
                    //只有当条件定时器触发或者有事件触发了才会跳到这里
                    if (timer)
                    {
                        timer->cancel();
                    }
                    //如果是由于超时导致的问题，设置errno直接返回
                    if (tinfo->canceled)
                    {
                        errno = tinfo->canceled;
                        return -1;
                    }
                    //否则表示有可用的io操作，在下一次进行io是ret  > 0直接会终止
                }
            }
            else
            {
                break;
            }
        }
        return ret;
    }
}

extern "C"{
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX
    unsigned int sleep(unsigned int seconds)
    {
        //没有hook，直接调用系统调用
        if (!sylar::t_hook_enable)
        {
            return sleep_f(seconds);
        }
        //否则设置定时器使当前协程在seconds后唤醒
        auto fiber = sylar::Fiber::GetCurFiber();
        auto iom = sylar::IOManager::get_this();
        iom->add_timer(seconds * 1000,
            [iom,fiber]()
            {
                iom->schedule(fiber,-1);
            });
        sylar::Fiber::yied_to_hold();
        return 0;
    }
    int usleep(useconds_t useconds)
    {
        if (!sylar::t_hook_enable)
        {
            return usleep_f(useconds);
        }
        auto fiber = sylar::Fiber::GetCurFiber();
        auto iom = sylar::IOManager::get_this();
        iom->add_timer(useconds / 1000,
            [iom,fiber]()
            {
                iom->schedule(fiber,-1);
            });
        sylar::Fiber::yied_to_hold();
        return 0;
    }

    int nanosleep(const struct timespec *req, struct timespec *rem)
    {
        if (!sylar::t_hook_enable)
        {
            return nanosleep(req,rem);
        }
        auto fiber = sylar::Fiber::GetCurFiber();
        auto iom = sylar::IOManager::get_this();
        iom->add_timer(req->tv_sec * 1000 + req->tv_nsec / 1000000,
            [iom,fiber]()
            {
                iom->schedule(fiber,-1);
            });
        sylar::Fiber::yied_to_hold();
        return 0;
    }

    int socket(int domain, int type, int protocol)
    {
        if (!sylar::t_hook_enable)
        {
            return socket_f(domain, type, protocol);
        }
        int fd = socket_f(domain, type, protocol);
        if (fd == -1)
        {
            return fd;
        }
        //将当前文件描述符加入到fd_manager中
        sylar::FdManager::GetInstance()->get_fd(fd, true);
        return fd;
    }

    int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen,uint64_t timeout_ms)
    {
        if (!sylar::t_hook_enable)
        {
            return connect_f(fd, addr, addrlen);
        }
        auto ctx = sylar::FdManager::GetInstance()->get_fd(fd, true);
        if (!ctx || ctx->is_closed())
        {
            errno = EBADF;
            return -1;
        }
        if (!ctx->is_socket() || ctx->get_user_block())
        {
            return connect_f(fd, addr, addrlen);
        }
        int n =connect_f(fd, addr, addrlen);
        //  n == 0 表示链接成功
        if (n == 0)
        {
            return 0;
        }//表示错误
        else if (n != -1 && errno != EINPROGRESS)
        {
            return n;
        }
        //后面表示正在处理中
        auto iom = sylar::IOManager::get_this();
        sylar::Timer::ptr timer;//条件定时器
        std::shared_ptr<sylar::timer_info> tinfo(new sylar::timer_info);
        std::weak_ptr<sylar::timer_info> winfo(tinfo);

        if (timeout_ms != static_cast<uint64_t>(-1))
        {
            timer = iom->add_condition_timer(timeout_ms,[winfo,fd,iom]
            {
                auto t = winfo.lock();
                if (!t || t->canceled)
                {
                    return;
                }
                t->canceled = ETIMEDOUT;
                iom->cancel_event(fd,sylar::IOManager::Write);
            },winfo);
        }

        int rt = iom->add_event(fd,sylar::IOManager::Write);
        //添加事件成功
        if (rt == 0)
        {
            sylar::Fiber::yied_to_hold();
            //超时定时器还没有执行，取消
            if (timer)
            {
                timer->cancel();
            }
            //若超时了
            if (tinfo->canceled)
            {
                errno = tinfo->canceled;
                return -1;
            }
        }else
        {
            if (timer)
            {
                timer->cancel();
            }
            LOG_ROOT_ERROR() << "connect addEvent(" << fd << ", "
                << static_cast<int>(sylar::IOManager::Write) << ") error";
        }
        //获取错误码
        int error = 0;
        if (-1 == getsockopt_f(fd,SOL_SOCKET,SO_ERROR,&error,&addrlen))
        {
            return -1;
        }
        if (!error)
        {
            return 0;
        }
        else
        {
            errno = error;
            return -1;
        }
    }

    int connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
    {
        return connect_with_timeout(fd,addr,addrlen,sylar::s_connect_timeout);
    }

    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
    {
        int fd = sylar::do_io(sockfd,accept_f,"accept",
            sylar::IOManager::Read,SO_RCVTIMEO,addr,addrlen);
        if (fd > 0)
        {
            sylar::FdManager::GetInstance()->get_fd(fd,true);
        }
        return fd;
    }

    ssize_t read(int fd, void *buf, size_t count)
    {
        return sylar::do_io(fd, read_f,"read",sylar::IOManager::Read,SO_RCVTIMEO,buf,count);
    }

    ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
    {
        return sylar::do_io(fd,readv_f,"readv",sylar::IOManager::Read,SO_RCVTIMEO,iov,iovcnt);
    }

    ssize_t recv(int fd, void *buf, size_t count, int flags)
    {
        return sylar::do_io(fd,recv_f,"recv",sylar::IOManager::Read,SO_RCVTIMEO,buf,count,flags);
    }

    ssize_t recvfrom(int fd, void *buf, size_t count, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
    {
        return sylar::do_io(fd,recvfrom_f,"recvfrom",sylar::IOManager::Read,SO_RCVTIMEO,
            buf,count,flags,src_addr,addrlen);
    }

    ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
    {
        return sylar::do_io(fd,recvmsg_f,"recvmsg",sylar::IOManager::Read,SO_RCVTIMEO,
            msg,flags);
    }
    ssize_t write(int fd, const void *buf, size_t count)
    {
        return sylar::do_io(fd,write_f,"write",sylar::IOManager::Write,SO_SNDTIMEO,buf,count);
    }
    ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
    {
        return sylar::do_io(fd,writev_f,"writev",sylar::IOManager::Write,SO_SNDTIMEO,iov,iovcnt);
    }
    ssize_t send(int fd, const void *buf, size_t len, int flags)
    {
        return sylar::do_io(fd,send_f,"send",sylar::IOManager::Write,SO_SNDTIMEO,buf,len,flags);
    }
    ssize_t sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
    {
        return sylar::do_io(fd,sendto_f,"sendto",sylar::IOManager::Write,SO_SNDTIMEO,
            buf,len,flags,dest_addr,addrlen);
    }
    ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
    {
        return sylar::do_io(fd,sendmsg_f,"sendmsg",sylar::IOManager::Write,SO_SNDTIMEO,
            msg,flags);
    }
    int close(int fd)
    {
       if (!sylar::t_hook_enable)
       {
           return close(fd);
       }
        auto ctx = sylar::FdManager::GetInstance()->get_fd(fd);
        if (ctx)
        {
            auto iom = sylar::IOManager::get_this();
            if (iom)
            {
                iom->cancel_all(fd);
            }
            sylar::FdManager::GetInstance()->del_fd(fd);
        }
        return close_f(fd);
    }

    int fcntl(int fd, int cmd, ... /* arg */ ) {
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                int arg = va_arg(va, int);
                va_end(va);
                sylar::FdCtx::ptr ctx = sylar::FdManager::GetInstance()->get_fd(fd);
                if(!ctx || ctx->is_closed() || !ctx->is_socket()) {
                    return fcntl_f(fd, cmd, arg);
                }
                ctx->set_user_block(arg & O_NONBLOCK);
                if(ctx->get_sys_nonblock()) {
                    arg |= O_NONBLOCK;
                } else {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                sylar::FdCtx::ptr ctx = sylar::FdManager::GetInstance()->get_fd(fd);
                if(!ctx || ctx->is_closed() || !ctx->is_socket()) {
                    return arg;
                }
                if(ctx->get_user_block()) {
                    return arg | O_NONBLOCK;
                } else {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request) {
        bool user_nonblock = !!*(int*)arg;
        sylar::FdCtx::ptr ctx = sylar::FdManager::GetInstance()->get_fd(d);
        if(!ctx || ctx->is_closed() || !ctx->is_socket()) {
            return ioctl_f(d, request, arg);
        }
        ctx->set_user_block(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if(!sylar::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET) {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            sylar::FdCtx::ptr ctx = sylar::FdManager::GetInstance()->get_fd(sockfd);
            if(ctx) {
                const timeval* v = (const timeval*)optval;
                ctx->set_timeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}
}
