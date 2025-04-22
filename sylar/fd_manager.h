//
// Created by 张羽 on 25-4-21.
//

#ifndef FD_MANAGER_H
#define FD_MANAGER_H
#include <memory>
#include <vector>

#include "mutex.h"
#include "Singleton.h"

namespace sylar
{
    class FdCtx : public std::enable_shared_from_this<FdCtx>
    {
    public:
        using ptr = std::shared_ptr<FdCtx>;
        explicit FdCtx(int fd);
        ~FdCtx();
        bool is_init() const { return m_init; }
        bool is_socket() const { return m_isSocket; }
        bool is_closed() const { return m_isClose; }
        void set_user_block(bool v) { m_userNonblock = v; }
        bool get_user_block() const { return m_userNonblock; }
        void set_sys_nonblock(bool v) { m_sysNonblock = v; }
        bool get_sys_nonblock() const { return m_sysNonblock; }
        void set_timeout(int type, uint64_t v);
        uint64_t get_timeout(int type);

    private:
        bool init();
    private:
        bool m_init : 1;//是否初始化
        bool m_isSocket : 1;//是否是socket
        bool m_sysNonblock : 1;//是否是非阻塞 hook导致的非阻塞
        bool m_userNonblock : 1;//是否是非阻塞 用户设置的非阻塞
        bool m_isClose : 1;//是否关闭
        int m_fd;//文件描述符
        uint64_t m_recvTimeout;//接收超时
        uint64_t m_sendTimeout;//发送超时
    };
    class FdManager : public Singleton<FdManager>
    {
    public:
        FdManager();
        FdCtx::ptr get_fd(int fd,bool auto_create = false);
        void del_fd(int fd);
    private:
        Mutex m_mutex;
        std::vector<FdCtx::ptr> m_ctxs;
    };
}

#endif //FD_MANAGER_H
