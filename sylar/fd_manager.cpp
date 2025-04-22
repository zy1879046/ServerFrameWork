//
// Created by 张羽 on 25-4-21.
//
#include "fd_manager.h"
#include "hook.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
namespace sylar
{
    FdCtx::FdCtx(int fd)
        :m_init(false),
        m_isSocket(false),
        m_sysNonblock(false),
        m_userNonblock(false),
        m_fd(fd),
        m_recvTimeout(-1),
        m_sendTimeout(-1)
    {
        init();
    }

    FdCtx::~FdCtx()
    {
    }

    void FdCtx::set_timeout(int type, uint64_t v)
    {
        if (type == SO_RCVTIMEO)
        {
            m_recvTimeout = v;
        }else if (type == SO_SNDTIMEO)
        {
            m_sendTimeout = v;
        }
    }

    uint64_t FdCtx::get_timeout(int type)
    {
        if (type == SO_RCVTIMEO)
        {
            return m_recvTimeout;
        }else
        {
            return m_sendTimeout;
        }
    }

    bool FdCtx::init()
    {
        if (m_init)return true;
        //设置超时时间，默认最大
        m_recvTimeout = -1;
        m_sendTimeout = -1;
        struct stat fd_stat;
        //如果当前文件描述符还没有初始化
        if (-1 == fstat(m_fd, &fd_stat))
        {
            m_init = false;
            m_isSocket = false;
        }else
        {
            m_init = true;
            m_isSocket = S_ISSOCK(fd_stat.st_mode);
        }
        if (m_isSocket)
        {
            int flags = fcntl_f(m_fd, F_GETFD,0);
            //如果当前为阻塞的
            if (!(flags & O_NONBLOCK))
            {
                fcntl_f(m_fd, F_SETFD, flags | O_NONBLOCK);
            }
            m_sysNonblock = true;
        }else
        {
            m_sysNonblock = false;
        }
        m_userNonblock = false;
        m_isClose = false;
        return m_init;
    }

    FdManager::FdManager()
    {
        m_ctxs.resize(64);
    }

    FdCtx::ptr FdManager::get_fd(int fd, bool auto_create)
    {
        if (fd == -1)return nullptr;
        UniqueLock<Mutex> lock(m_mutex);
        //处理不自动创建的情况
        if (static_cast<size_t>(fd) >= m_ctxs.size())
        {
            if (!auto_create)return nullptr;
        }else
        {
            //存在或者不自动创建
            if (m_ctxs[fd] || !auto_create)
            {
                return m_ctxs[fd];
            }
        }
        lock.unlock();
        //处理自动创建的情况
        LockGuard<Mutex> guard(m_mutex);
        FdCtx::ptr ctx(new FdCtx(fd));
        if (fd >= static_cast<int>(m_ctxs.size()))
        {
            m_ctxs.resize(fd * 1.5);
        }
        m_ctxs[fd] = ctx;
        return ctx;
    }

    void FdManager::del_fd(int fd)
    {
        LockGuard<Mutex> guard(m_mutex);
        if (static_cast<int>(m_ctxs.size()) <= fd)
        {
            return;
        }
        m_ctxs[fd].reset();
    }
}
