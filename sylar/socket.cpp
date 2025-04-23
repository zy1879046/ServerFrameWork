//
// Created by 张羽 on 25-4-22.
//
#include "socket.h"

#include <memory>

#include "fd_manager.h"
#include "hook.h"
#include "iomanager.h"
#include "log.h"

namespace sylar
{
    Socket::ptr Socket::CreateTCP(Address::ptr addr)
    {
        Socket::ptr sock(new Socket(addr->get_family(), TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDP(Address::ptr addr)
    {
        Socket::ptr sock(new Socket(addr->get_family(), UDP, 0));
        sock->newSocket();
        sock->m_isConnected = true;
        return sock;
    }

    Socket::ptr Socket::CreateTCPSocket()
    {
        Socket::ptr sock(new Socket(IPV4, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDPSocket()
    {
        Socket::ptr sock(new Socket(IPV4, UDP, 0));
        sock->newSocket();
        sock->m_isConnected = true;
        return sock;
    }

    Socket::ptr Socket::CreateTCPSocket6()
    {
        Socket::ptr sock(new Socket(IPV6, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUDPSocket6()
    {
        Socket::ptr sock(new Socket(IPV6, UDP, 0));
        sock->newSocket();
        sock->m_isConnected = true;
        return sock;
    }

    Socket::ptr Socket::CreateUnixTCPSocket()
    {
        Socket::ptr sock(new Socket(UNIX, TCP, 0));
        return sock;
    }

    Socket::ptr Socket::CreateUnixUDPSocket()
    {
        Socket::ptr sock(new Socket(UNIX, UDP, 0));
        return sock;
    }

    Socket::Socket(int family, int type, int protocol)
        :m_sock(-1),m_family(family), m_type(type), m_protocol(protocol),m_isConnected(false)
    {
    }

    Socket::~Socket()
    {
        Socket::close();
    }

    int64_t Socket::getSendTimeout() const
    {
        FdCtx::ptr ctx = FdManager::GetInstance()->get_fd(m_sock);
        if (ctx)
        {
            return static_cast<int64_t>(ctx->get_timeout(SO_SNDTIMEO));
        }
        return -1;
    }

    int64_t Socket::getRecvTimeout() const
    {
        FdCtx::ptr ctx = FdManager::GetInstance()->get_fd(m_sock);
        if (ctx)
        {
            return static_cast<int64_t>(ctx->get_timeout(SO_RCVTIMEO));
        }
        return -1;
    }

    void Socket::setSendTimeout(int64_t v)
    {
        struct timeval tv{};
        tv.tv_sec = v / 1000;
        tv.tv_usec = (v % 1000) * 1000;
        setOption(SOL_SOCKET, SO_SNDTIMEO, &tv);
    }

    void Socket::setRecvTimeout(int64_t v)
    {
        struct timeval tv{};
        tv.tv_sec = v / 1000;
        tv.tv_usec = (v % 1000) * 1000;
        setOption(SOL_SOCKET, SO_RCVTIMEO, &tv);
    }

    bool Socket::getOption(int level, int option, void* result, size_t* len) const
    {
        int rt = getsockopt(m_sock, level, option, result, (socklen_t*)len);
        if (rt)
        {
            LOG_SYSTEM_DEBUG() << "getOption(" << m_sock << ", " << level << ", " << option
                << ") rt=" << rt << " errno=" << errno
                << " errstr=" << strerror(errno);
            return false;
        }
        return true;
    }

    bool Socket::setOption(int level, int option, const void* result, size_t len)
    {
        int rt = setsockopt(m_sock, level, option, result, len);
        if (rt)
        {
            LOG_SYSTEM_DEBUG() << "setOption(" << m_sock << ", " << level << ", " << option
                << ") rt=" << rt << " errno=" << errno
                << " errstr=" << strerror(errno);
            return false;
        }
        return true;
    }

    Socket::ptr Socket::accept()
    {
        Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
        int newsock = ::accept(m_sock, nullptr, nullptr);
        if (newsock == -1)
        {
            LOG_SYSTEM_DEBUG() << "Socket::accept failed";
            return nullptr;
        }
        if (sock->init(newsock))
        {
            return sock;
        }
        return nullptr;
    }

    bool Socket::bind(const Address::ptr addr)
    {
        if (!isValid())
        {
            newSocket();
            if (!isValid())
            {
                return false;
            }
        }
        if (addr->get_family() != m_family)
        {
            LOG_SYSTEM_DEBUG() << "bind sock.family("
            << m_family << ") add.family(" << addr->get_family()
            << ") not qual, addr = " << addr->to_string();
            return false;
        }
        if (::bind(m_sock,addr->get_addr(),addr->get_addrlen()) == -1)
        {
            LOG_SYSTEM_DEBUG() << "bind(" << m_sock << ", " << addr->to_string()
                << ") errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }
        getLocalAddress();
        return true;
    }

    bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms)
    {
        //如果当前socket没有连接
        if (!isValid())
        {
            newSocket();
            if (!isValid())
            {
                return false;
            }
        }
        if (addr->get_family() != m_family)
        {
            LOG_SYSTEM_DEBUG() << "connect sock.family("
                << m_family << ") add.family(" << addr->get_family()
                << ") not qual, addr = " << addr->to_string();
            return false;
        }
        if (timeout_ms == static_cast<uint64_t>(-1))
        {
            if (::connect(m_sock, addr->get_addr(), addr->get_addrlen()))
            {
                LOG_SYSTEM_DEBUG() <<"connect(" << m_sock << ", " << addr->to_string()
                    << ") errno=" << errno << " errstr=" << strerror(errno);
                close();
                return false;
            }
        }else
        {
            if (connect_with_timeout(m_sock, addr->get_addr(),addr->get_addrlen() ,timeout_ms))
            {
                LOG_SYSTEM_DEBUG() << "connect(" << m_sock << ", " << addr->to_string()
                    << ") errno=" << errno << " errstr=" << strerror(errno);
                close();
                return false;
            }
        }
        m_isConnected = true;
        getLocalAddress();
        getRemoteAddress();
        return true;

    }

    bool Socket::reconnect(uint64_t timeout_ms)
    {
        if(!m_remoteAddr) {
            LOG_SYSTEM_DEBUG() << "reconnect m_remoteAddress is null";
            return false;
        }
        m_localAddr.reset();
        return connect(m_remoteAddr, timeout_ms);
    }

    bool Socket::listen(int backlog)
    {
        if (!isValid())
        {
            LOG_SYSTEM_DEBUG() << "listen sock = -1";
            return false;
        }
        if (::listen(m_sock, backlog))
        {
            LOG_SYSTEM_DEBUG() << "listen(" << m_sock << ", " << backlog
                << ") errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }
        return true;
    }

    bool Socket::close()
    {
        if (!m_isConnected && m_sock == -1)
        {
            return true;
        }
        m_isConnected = true;
        if (m_sock != -1)
        {
            ::close(m_sock);
            m_sock = -1;
        }
        return true;

    }

    int Socket::send(const void* data, size_t len, int flags)
    {
        if (m_isConnected)
        {
            return ::send(m_sock, data, len, flags);
        }
        return -1;
    }

    int Socket::send(const iovec* data, size_t len, int flags)
    {
        if (m_isConnected)
        {
            msghdr msg = {};
            msg.msg_iov = const_cast<iovec*>(data);
            msg.msg_iovlen = len;
            return static_cast<int>(::sendmsg(m_sock, &msg, flags));
        }
        return -1;
    }

    int Socket::sendTo(const void* data, size_t len, const Address::ptr to, int flags)
    {
        if (m_isConnected)
        {
            return ::sendto(m_sock, data, len, flags, to->get_addr(), to->get_addrlen());
        }
        return -1;
    }

    int Socket::sendTo(const iovec* data, size_t len, const Address::ptr to, int flags)
    {
        if (m_isConnected)
        {
            msghdr msg = {};
            msg.msg_iov = const_cast<iovec*>(data);
            msg.msg_iovlen = len;
            return static_cast<int>(::sendmsg(m_sock, &msg, flags));
        }
        return -1;
    }

    int Socket::recv(void* data, size_t len, int flags)
    {
        if (m_isConnected)
        {
            return static_cast<int>(::recv(m_sock, data, len, flags));
        }
        return -1;
    }

    int Socket::recv(iovec* data, size_t len, int flags)
    {
        if (m_isConnected)
        {
            msghdr msg = {};
            msg.msg_iov = const_cast<iovec*>(data);
            msg.msg_iovlen = len;
            return static_cast<int>(::recvmsg(m_sock, &msg, flags));
        }
        return -1;
    }

    int Socket::recvFrom(void* data, size_t len, const Address::ptr to, int flags)
    {
        if (m_isConnected)
        {
            auto len = to->get_addrlen();
            return static_cast<int>(::recvfrom(m_sock,data,len,flags, to->get_addr(), &len));
        }
        return -1;
    }

    int Socket::recvFrom(iovec* data, size_t len, const Address::ptr to, int flags)
    {
        if (m_isConnected)
        {
            msghdr msg = {};
            msg.msg_iov = const_cast<iovec*>(data);
            msg.msg_iovlen = len;
            return static_cast<int>(::recvmsg(m_sock, &msg, flags));
        }
        return -1;
    }

    Address::ptr Socket::getLocalAddress()
    {
        if (m_localAddr)return m_localAddr;
        Address::ptr addr;
        switch (m_family)
        {
        case IPV4:
            addr = std::make_shared<IPv4Address>();
            break;
        case IPV6:
            addr = std::make_shared<IPv6Address>();
            break;
        case UNIX:
            addr = std::make_shared<UnixAddress>();
            break;
        default:
            addr = std::make_shared<UnknownAddress>(m_family);
            break;
        }
        socklen_t addrlen = addr->get_addrlen();
        if (getsockname(m_sock, addr->get_addr(), &addrlen))
        {
            LOG_SYSTEM_DEBUG() << "getsockname(" << m_sock << ", " << addrlen
                << ") errno=" << errno << " errstr=" << strerror(errno);
            return std::make_shared<UnknownAddress>(m_family);
        }
        if (m_family == UNIX)
        {
            UnixAddress::ptr unix_addr = std::make_shared<UnixAddress>();
            unix_addr->set_addrLen(addrlen);
        }
        m_localAddr = addr;
        return m_localAddr;
    }

    Address::ptr Socket::getRemoteAddress()
    {
        if (m_remoteAddr)
        {
            return m_remoteAddr;
        }
        Address::ptr addr;
        switch (m_family)
        {
        case IPV4:
            addr = std::make_shared<IPv4Address>();
            break;
        case IPV6:
            addr = std::make_shared<IPv6Address>();
            break;
        case UNIX:
            addr = std::make_shared<UnixAddress>();
            break;
        default:
            addr = std::make_shared<UnknownAddress>(m_family);
            break;
        }
        socklen_t len = addr->get_addrlen();
        //获取远程地址 存在addr中
        if (::getpeername(m_sock, addr->get_addr(), &len))
        {
            LOG_SYSTEM_DEBUG() << "getpeername(" << m_sock << ") errno=" << errno
                << " errstr=" << strerror(errno);
            return std::make_shared<UnknownAddress>(m_family);
        }
        if (m_family == UNIX)
        {
            UnixAddress::ptr unix_addr = std::dynamic_pointer_cast<UnixAddress>(addr);
            unix_addr->set_addrLen(len);
        }
        m_remoteAddr = addr;
        return m_remoteAddr;
    }

    int Socket::getFamily() const
    {
        return m_family;
    }

    int Socket::getType() const
    {
        return m_type;
    }

    int Socket::getProtocol() const
    {
        return m_protocol;
    }

    bool Socket::isConnected() const
    {
        return m_isConnected;
    }

    bool Socket::isValid() const
    {
        return m_sock != -1;
    }

    int Socket::getError() const
    {
        int error = 0;
        size_t len = sizeof(error);
        if (getOption(SOL_SOCKET, SO_ERROR, &error,&len))
        {
            error = errno;
        }
        return error;
    }

    std::ostream& Socket::dump(std::ostream& os) const
    {
        os << "[Socket sock=" << m_sock
            << " family=" << m_family
            << " type=" << m_type
            << " protocol=" << m_protocol
            << " isConnected=" << m_isConnected;
        if (m_localAddr)
        {
            os << " localAddress=" << m_localAddr->to_string();
        }
        if (m_remoteAddr)
        {
            os << " remoteAddress=" << m_remoteAddr->to_string();
        }
        os << "]";
        return os;
    }

    std::string Socket::toString() const
    {
        std::stringstream ss;
        dump(ss);
        return ss.str();
    }

    bool Socket::cancelRead() const
    {
        return IOManager::get_this()->cancel_event(m_sock,IOManager::Read);
    }

    bool Socket::cancelWrite() const
    {
        return IOManager::get_this()->cancel_event(m_sock,IOManager::Write);
    }

    bool Socket::cancelAccept() const
    {
        return IOManager::get_this()->cancel_event(m_sock,IOManager::Read);
    }

    bool Socket::cancelAll() const
    {
        return IOManager::get_this()->cancel_all(m_sock);
    }

    void Socket::initSocket()
    {
        int val = 1;
        // SO_REUSEADDR 打开或关闭地址复用功能 option_value不等于0时，打开
        setOption(SOL_SOCKET, SO_REUSEADDR, val);
        if (m_type == TCP)
        {
            // Nagle算法通过将小数据块合并成更大的数据块来减少网络传输的次数，提高网络传输的效率。
            // 禁用Nagle算法，减小延迟
            setOption(IPPROTO_TCP, TCP_NODELAY, val);
        }
    }

    void Socket::newSocket()
    {
        m_sock = socket(m_family, m_type, m_protocol);

        if (isValid())
        {
            initSocket();
        }else
        {
            LOG_SYSTEM_DEBUG() << "socket(" << m_sock << ", " << m_family
                << ", " << m_type << ", " << m_protocol
                << ") errno=" << errno << " errstr=" << strerror(errno);
        }
    }

    bool Socket::init(int sock)
    {
        /*
         * 创建套接字是否应由FdManager管理，如果是 应该使用 自动创建，否则不会被管理。
         */
        FdCtx::ptr ctx = FdManager::GetInstance()->get_fd(sock);
        // FdCtx::ptr ctx = FdManager::GetInstance()->get_fd(sock,true);
        if (ctx && ctx->is_socket() && !ctx->is_closed())
        {
            m_sock = sock;
            m_isConnected = true;
            initSocket();
            getLocalAddress();
            getRemoteAddress();
            return true;
        }
        return false;
    }
}
