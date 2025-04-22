//
// Created by 张羽 on 25-4-22.
//

#ifndef ADDRESS_H
#define ADDRESS_H
#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <map>
namespace sylar
{
    class IPAddress;

    /**
     *所有地址的基类
     */
    class Address
    {
    public:
        enum AddressType
        {
            IPV4 = AF_INET,
            IPV6 = AF_INET6,
            UNIX = AF_UNIX,
            UNKNOWN = AF_UNSPEC
        };
        using ptr = std::shared_ptr<Address>;
        /**
         * 通过sockaddr指针创建地址
         * @param addr
         * @param addrlen
         * @return
         */
        static Address::ptr create(const sockaddr* addr, socklen_t addrlen);

        /**
         * 通过host返回所有满足条件的地址
         * @param result 保存所有满足条件的地址
         * @param host 域名，服务器等
         * @param family 协议族 IPV4,IPV6,UNIX
         * @param type socket类型 SOCK_STREAM,SOCK_DGRAM
         * @param protocol 协议类型 IPPROTO_TCP,IPPROTO_UDP
         * @return
         */
        static bool lookup(std::vector<Address::ptr>& result,const std::string& host,
                           int family = IPV4,int type = 0,int protocol = 0);

        /**
         * 返回任意一个地址
         * @param host
         * @param family
         * @param type
         * @param protocol
         * @return
         */
        static Address::ptr lookup_any(const std::string& host,int family = IPV4,int type = 0,int protocol = 0);

        /**
         * 返回任意一个Ip address
         * @param host
         * @param family
         * @param type
         * @param protocol
         * @return
         */
        static std::shared_ptr<IPAddress> lookup_any_ip_address(const std::string& host,
                                                        int family = IPV4,int type = 0,int protocol = 0);
        /**
         * 获取本机所有网卡的 <网卡名，地址，子网掩码数>
         * @param result
         * @param family
         * @return
         */
        static bool get_interface_address(std::multimap<std::string,std::pair<Address::ptr,uint32_t>>& result,
                                          int family = IPV4);

        /**
         * 获取指定网卡的 <地址，子网掩码数>
         * @param result
         * @param iface
         * @param family
         * @return
         */
        static bool get_interface_address(std::vector<std::pair<Address::ptr,uint32_t>>& result,
                                          const std::string& iface,int family = IPV4);
        virtual ~Address() = default;

        int get_family() const;

        virtual const sockaddr* get_addr() const = 0;

        virtual sockaddr* get_addr() = 0;

        virtual socklen_t get_addrlen() const = 0;

        virtual std::ostream& insert(std::ostream& os) const = 0;

        std::string to_string() const;

        bool operator==(const Address& rhs) const;

        bool operator!=(const Address& rhs) const;

        bool operator<(const Address& rhs) const;
    };

    class IPAddress: public Address
    {
    public:
        using ptr = std::shared_ptr<IPAddress>;

        static IPAddress::ptr create(const std::string& host,uint16_t port = 0);

        virtual ~IPAddress() = default;
        //获取广播地址
        virtual IPAddress::ptr broadcast_address(uint32_t prefix_len) const = 0;
        //获取网段
        virtual IPAddress::ptr network_address(uint32_t prefix_len) const = 0;
        //获取子网掩码
        virtual IPAddress::ptr subnet_mask(uint32_t prefix_len) const = 0;

        virtual uint16_t get_port() const = 0;

        virtual void set_port(uint16_t port) = 0;
    };

    class IPv4Address: public IPAddress
    {
    public:
        using ptr = std::shared_ptr<IPv4Address>;

        static IPv4Address::ptr create(const std::string& host,uint16_t port = 0);
        IPv4Address(const sockaddr_in& addr);

        //构键本机 IP 地址
        IPv4Address(uint32_t address = INADDR_ANY,uint16_t port = 0);

        const sockaddr* get_addr() const override;
        sockaddr* get_addr() override;
        socklen_t get_addrlen() const override;
        std::ostream& insert(std::ostream& os) const override;
        uint16_t get_port() const override;
        void set_port(uint16_t port) override;
        IPAddress::ptr broadcast_address(uint32_t prefix_len) const override;
        IPAddress::ptr network_address(uint32_t prefix_len) const override;
        IPAddress::ptr subnet_mask(uint32_t prefix_len) const override;

    private:
        sockaddr_in m_addr;
    };

    class IPv6Address: public IPAddress
    {
    public:
        using ptr = std::shared_ptr<IPv6Address>;

        static IPv6Address::ptr create(const std::string& host,uint16_t port = 0);
        IPv6Address(const sockaddr_in6& addr);

        //构键本机 IP 地址
        IPv6Address();

        IPv6Address(const uint8_t addr[16],uint16_t port = 0);
        const sockaddr* get_addr() const override;
        sockaddr* get_addr() override;
        socklen_t get_addrlen() const override;
        std::ostream& insert(std::ostream& os) const override;
        uint16_t get_port() const override;
        void set_port(uint16_t port) override;
        IPAddress::ptr broadcast_address(uint32_t prefix_len) const override;
        IPAddress::ptr network_address(uint32_t prefix_len) const override;
        IPAddress::ptr subnet_mask(uint32_t prefix_len) const override;

    private:
        sockaddr_in6 m_addr;
    };

    class UnixAddress : public Address {
    public:
        typedef std::shared_ptr<UnixAddress> ptr;

        /**
         * @brief 无参构造函数
         */
        UnixAddress();

        /**
         * @brief 通过路径构造UnixAddress
         * @param[in] path UnixSocket路径(长度小于UNIX_PATH_MAX)
         */
        UnixAddress(const std::string& path);

        const sockaddr* get_addr() const override;
        sockaddr* get_addr() override;
        socklen_t get_addrlen() const override;
        void set_addrLen(uint32_t v);
        std::string getPath() const;
        std::ostream& insert(std::ostream& os) const override;
    private:
        sockaddr_un m_addr;
        socklen_t m_length;
    };



    /**
 * @brief 未知地址
 */
    class UnknownAddress : public Address {
    public:
        typedef std::shared_ptr<UnknownAddress> ptr;
        explicit UnknownAddress(int family);
        explicit UnknownAddress(const sockaddr& addr);
        const sockaddr* get_addr() const override;
        sockaddr* get_addr() override;
        socklen_t get_addrlen() const override;
        std::ostream& insert(std::ostream& os) const override;
    private:
        sockaddr m_addr;
    };

    /**
     * @brief 流式输出Address
     */
    std::ostream& operator<<(std::ostream& os, const Address& addr);


}
#endif //ADDRESS_H
