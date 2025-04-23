//
// Created by 张羽 on 25-4-22.
//

#ifndef SOCKET_H
#define SOCKET_H
#include <memory>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
// #include <openssl/err.h>
// #include <openssl/ssl.h>
#include "address.h"
#include "noncopyable.h"

namespace sylar
{
    class Socket : public std::enable_shared_from_this<Socket>,NonCopyable
    {
    public:
        using ptr = std::shared_ptr<Socket>;
        using weak_ptr = std::weak_ptr<Socket>;
        enum Type
        {
            TCP = SOCK_STREAM,
            UDP = SOCK_DGRAM,
        };
        enum Family
        {
            IPV4 = AF_INET,
            IPV6 = AF_INET6,
            UNIX = AF_UNIX,
        };

        static Socket::ptr CreateTCP(Address::ptr addr);
        static Socket::ptr CreateUDP(Address::ptr addr);
        static Socket::ptr CreateTCPSocket();
        static Socket::ptr CreateUDPSocket();
        static Socket::ptr CreateTCPSocket6();
        static Socket::ptr CreateUDPSocket6();
        static Socket::ptr CreateUnixTCPSocket();
        static Socket::ptr CreateUnixUDPSocket();

        Socket(int family, int type, int protocol);
        virtual ~Socket();
        int64_t getSendTimeout() const;
        int64_t getRecvTimeout() const;
        void setSendTimeout(int64_t v);
        void setRecvTimeout(int64_t v);

        bool getOption(int level, int option, void* result, size_t* len) const;
        template<typename T>
        bool getOption(int level, int option, T& result) const
        {
            size_t len = sizeof(T);
            return getOption(level, option, &result, &len);
        }

        bool setOption(int level, int option, const void* result, size_t len);
        template<typename T>
        bool setOption(int level, int option, const T& result)
        {
            return setOption(level, option, &result, sizeof(T));
        }

        virtual Socket::ptr accept();
        virtual bool bind(const Address::ptr addr);
        virtual bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
        virtual bool reconnect(uint64_t timeout_ms = -1);
        virtual bool listen(int backlog = SOMAXCONN);
        virtual bool close();
        //TCP
        virtual int send(const void* data, size_t len, int flags = 0);
        virtual int send(const iovec* data, size_t len, int flags = 0);
        //UDP
        virtual int sendTo(const void* data, size_t len, const Address::ptr to, int flags = 0);
        virtual int sendTo(const iovec* data, size_t len, const Address::ptr to, int flags = 0);

        virtual int recv(void* data, size_t len, int flags = 0);
        virtual int recv(iovec* data, size_t len, int flags = 0);

        virtual int recvFrom(void* data, size_t len, const Address::ptr to, int flags = 0);
        virtual int recvFrom(iovec* data, size_t len, const Address::ptr to, int flags = 0);

        Address::ptr getLocalAddress();
        Address::ptr getRemoteAddress();

        int getFamily() const;
        int getType() const;
        int getProtocol() const;
        bool isConnected() const;
        bool isValid() const;
        int getError() const;
        virtual std::ostream& dump(std::ostream& os) const;
        virtual std::string toString() const;
        int getSocket() const { return m_sock; }
        bool cancelRead() const;
        bool cancelWrite() const;
        bool cancelAccept() const;
        bool cancelAll() const;

    protected:
        void initSocket();
        void newSocket();
        bool init(int sock);
    private:
        int m_sock;
        int m_family;
        int m_type;
        int m_protocol;
        bool m_isConnected;
        Address::ptr m_localAddr;
        Address::ptr m_remoteAddr;
    };
}
#endif //SOCKET_H
