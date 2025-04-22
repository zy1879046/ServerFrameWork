//
// Created by 张羽 on 25-4-22.
//
#include "address.h"

#include "log.h"
#include <sstream>
#include <netdb.h>
#include <ifaddrs.h>
#include <stddef.h>
#include "myendian.h"
namespace sylar
{
    //也就是创建一个掩码为 00000000,00000000,00111111,11111111,这个代表前缀长10
    template<typename T>
    static T create_mask(uint32_t bits)
    {
        return (1 << (sizeof(T)*8 - bits)) - 1;
    }
    //统计字节中为1的个数
    template<typename T>
    static uint32_t count_bytes(T value)
    {
        uint32_t result = 0;
        for(; value; ++result) { // 循环直到 value 为 0
            value &= value - 1; // 每次将 value 的最低位的 1 置为 0
        }
        return result;
    }

    Address::ptr Address::create(const sockaddr* addr, socklen_t addrlen)
    {
        if (addr == nullptr)return nullptr;
        Address::ptr result;
        switch (addr->sa_family)
        {
        case IPV4:
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        case IPV6:
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        default:
            result.reset(new UnknownAddress(*(const sockaddr*)addr));
        }
        return result;
    }

    bool Address::lookup(std::vector<Address::ptr>& result, const std::string& host, int family, int type, int protocol)
    {
        addrinfo hints, *result_addr = nullptr, *next = nullptr;
        hints.ai_family = family;
        hints.ai_socktype = type;
        hints.ai_protocol = protocol;
        hints.ai_flags = 0;
        hints.ai_addrlen = 0;
        hints.ai_next = nullptr;
        hints.ai_canonname = nullptr;
        hints.ai_addr = nullptr;

        //存储ip地址
        std::string node;
        //存储端口
        const char* service = nullptr;
        //检查ipv6地址
        if (!host.empty() && host[0] == '[')
        {
            const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
            if (endipv6)
            {
                //检查是否有端口
                if (*(endipv6 + 1) == ':')
                {
                    service = endipv6 + 2;
                }
                node = host.substr(1, host.size() - 1);
            }
        }
        //如果不是ipv6地址,看是否为ipv4
        if (node.empty())
        {
            service = (const char*)memchr(host.c_str() + 1, ':', host.size() - 1);
            if (service){
                if(!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                    node = host.substr(0, service - host.c_str());
                    ++service;
                }
            }
        }
        //如果不是ipv4，那么应该是域名地址
        if (node.empty())
        {
            node = host;
        }

        int error = getaddrinfo(node.c_str(), service, &hints, &result_addr);
        if (error)
        {
            LOG_SYSTEM_DEBUG() << "Address::lookup(getaddrinfo" <<","<< gai_strerror(error);
            return false;
        }
        next = result_addr;
        while (next)
        {
            result.push_back(create(next->ai_addr, next->ai_addrlen));
            next = next->ai_next;
        }
        freeaddrinfo(result_addr);
        return !result.empty();
    }

    Address::ptr Address::lookup_any(const std::string& host, int family, int type, int protocol)
    {
        std::vector<Address::ptr> result;
        if (lookup(result, host, family, type, protocol))
        {
            return result[0];
        }
        return nullptr;
    }

    std::shared_ptr<IPAddress> Address::lookup_any_ip_address(const std::string& host, int family, int type,
        int protocol)
    {
        std::vector<Address::ptr> result;
        if (lookup(result, host, family, type, protocol))
        {
            for (auto& i : result)
            {
                auto v = std::dynamic_pointer_cast<IPAddress>(i);
                if (v)return v;
            }
        }
        return nullptr;
    }

    bool Address::get_interface_address(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& results,
        int family)
    {
        struct ifaddrs *next,*result;
        if (getifaddrs(&result) != 0)
        {
            LOG_SYSTEM_DEBUG() << "Address::get_interface_address(getifaddrs):" << strerror(errno);
            return false;
        }
        try
        {
            for (next = result; next != nullptr; next = next->ifa_next)
            {
                Address::ptr addr;
                uint32_t prefix_len = ~0u;
                if (family == UNKNOWN && family != next->ifa_addr->sa_family)
                {
                    continue;
                }
                //根据网卡类型进行处理
                switch (next->ifa_addr->sa_family)
                {
                case IPV4:
                    {
                        addr = create(next->ifa_addr,sizeof(sockaddr_in));
                        uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        prefix_len = count_bytes(netmask);
                    }
                    break;
                case IPV6:
                    {
                        addr = create(next->ifa_addr,sizeof(sockaddr_in6));
                        in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                        prefix_len = 0;
                        for (int i = 0 ; i < 16; ++i)
                        {
                            prefix_len += count_bytes(netmask.s6_addr[i]);
                        }
                    }
                    break;
                default:
                    break;
                }
                if (addr)
                {
                    results.insert(std::make_pair(next->ifa_name,std::make_pair(addr,prefix_len)));
                }
            }
        }catch (...)
        {
            freeifaddrs(result);
            LOG_SYSTEM_DEBUG() << "Address::get_interface_address exception";
            return false;
        }
        freeifaddrs(result);
        return !results.empty();
    }

    bool Address::get_interface_address(std::vector<std::pair<Address::ptr, uint32_t>>& result,
        const std::string& iface, int family)
    {
        if(iface.empty() || iface == "*") {
            if(family == AF_INET || family == AF_UNSPEC) {
                result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
            }
            if(family == AF_INET6 || family == AF_UNSPEC) {
                result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
            }
            return true;
        }

        std::multimap<std::string
              ,std::pair<Address::ptr, uint32_t> > results;

        if(!get_interface_address(results, family)) {
            return false;
        }

        auto its = results.equal_range(iface);
        for(; its.first != its.second; ++its.first) {
            result.push_back(its.first->second);
        }
        return !result.empty();
    }

    int Address::get_family() const
    {
        return get_addr()->sa_family;
    }

    std::string Address::to_string() const
    {
        std::stringstream ss;
        insert(ss);
        return ss.str();
    }

    bool Address::operator==(const Address& rhs) const
    {
        return get_addrlen() == rhs.get_addrlen() &&
            memcmp(get_addr(), rhs.get_addr(), get_addrlen()) == 0;
    }

    bool Address::operator!=(const Address& rhs) const
    {
        return !(*this == rhs);
    }

    bool Address::operator<(const Address& rhs) const
    {
        socklen_t minlen = std::min(get_addrlen(), rhs.get_addrlen());
        int result = memcmp(get_addr(), rhs.get_addr(), minlen);
        if(result < 0) {
            return true;
        } else if(result > 0) {
            return false;
        } else if(get_addrlen() < rhs.get_addrlen()) {
            return true;
        }
        return false;
    }

    IPAddress::ptr IPAddress::create(const std::string& host, uint16_t port)
    {
        addrinfo hints, *results;
        memset(&hints, 0, sizeof(addrinfo));

        hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_UNSPEC;

        int error = getaddrinfo(host.c_str(), NULL, &hints, &results);
        if(error) {
            LOG_SYSTEM_DEBUG() << "IPAddress::Create(" << host
                << ", " << port << ") error=" << error
                << " errno=" << errno << " errstr=" << strerror(errno);
            return nullptr;
        }

        try {
            IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
                    Address::create(results->ai_addr, (socklen_t)results->ai_addrlen));
            if(result) {
                result->set_port(port);
            }
            freeaddrinfo(results);
            return result;
        } catch (...) {
            freeaddrinfo(results);
            return nullptr;
        }
    }

    IPv4Address::ptr IPv4Address::create(const std::string& host, uint16_t port)
    {
        IPv4Address::ptr result(new IPv4Address());
        result->m_addr.sin_port = byteswapOnLittleEndian(port);
        int rt = inet_pton(IPV4, host.c_str(), &result->m_addr.sin_addr);
        if (rt <= 0)
        {
            LOG_SYSTEM_DEBUG() << "IPv4Address::create(): Failed to convert host to IPv4Address";
        }
        result->m_addr.sin_family = IPV4;
        return result;
    }

    IPv4Address::IPv4Address(const sockaddr_in& addr)
        :m_addr(addr)
    {
    }

    IPv4Address::IPv4Address(uint32_t address, uint16_t port)
    {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = IPV4;
        m_addr.sin_port = byteswapOnLittleEndian(port);
        m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
    }

    const sockaddr* IPv4Address::get_addr() const
    {
        return reinterpret_cast<const sockaddr*>(&m_addr);
    }

    sockaddr* IPv4Address::get_addr()
    {
        return reinterpret_cast<sockaddr*>(&m_addr);
    }

    socklen_t IPv4Address::get_addrlen() const
    {
        return sizeof(m_addr);
    }

    std::ostream& IPv4Address::insert(std::ostream& os) const
    {
        uint32_t ip = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
        os << ((ip >> 24) & 0xFF) << "."
            <<((ip >> 16) & 0xFF )<< "."
            <<((ip >>  8) & 0xFF )<< "."
            <<((ip >>  0) & 0xFF );
        os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
        return os;
    }

    uint16_t IPv4Address::get_port() const
    {
        return byteswapOnLittleEndian(m_addr.sin_port);
    }

    void IPv4Address::set_port(uint16_t port)
    {
        m_addr.sin_port = byteswapOnLittleEndian(port);
    }

    IPAddress::ptr IPv4Address::broadcast_address(uint32_t prefix_len) const
    {
        if (prefix_len > 32)return nullptr;
        sockaddr_in addr = m_addr;
        addr.sin_addr.s_addr |= byteswapOnLittleEndian(create_mask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(addr);
    }

    IPAddress::ptr IPv4Address::network_address(uint32_t prefix_len) const
    {
        if (prefix_len > 32)return nullptr;
        sockaddr_in addr = m_addr;
        addr.sin_addr.s_addr &= byteswapOnLittleEndian(~create_mask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(addr);
    }

    IPAddress::ptr IPv4Address::subnet_mask(uint32_t prefix_len) const
    {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = IPV4;
        addr.sin_addr.s_addr |= byteswapOnLittleEndian(~create_mask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(addr);

    }

    IPv6Address::ptr IPv6Address::create(const std::string& host, uint16_t port)
    {
        IPv6Address::ptr rt(new IPv6Address);
        rt->m_addr.sin6_port = byteswapOnLittleEndian(port);
        int result = inet_pton(AF_INET6, host.c_str(), &rt->m_addr.sin6_addr);
        if(result <= 0) {
            LOG_SYSTEM_DEBUG() << "IPv6Address::Create(" << host << ", "
                    << port << ") rt=" << result << " errno=" << errno
                    << " errstr=" << strerror(errno);
            return nullptr;
        }
        return rt;
    }

    IPv6Address::IPv6Address(const sockaddr_in6& addr)
        :m_addr(addr)
    {
    }

    IPv6Address::IPv6Address()
    {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = IPV6;
    }

    IPv6Address::IPv6Address(const uint8_t addr[16], uint16_t port)
    {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = IPV6;
        m_addr.sin6_port = byteswapOnLittleEndian(port);
        memcpy(m_addr.sin6_addr.s6_addr, addr, 16);
    }

    const sockaddr* IPv6Address::get_addr() const
    {
        return reinterpret_cast<const sockaddr*>(&m_addr);
    }

    sockaddr* IPv6Address::get_addr()
    {
        return reinterpret_cast<sockaddr*>(&m_addr);
    }

    socklen_t IPv6Address::get_addrlen() const
    {
        return sizeof(m_addr);
    }

    std::ostream& IPv6Address::insert(std::ostream& os) const
    {
        os << "[";
        uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
        bool used_zeros = false;
        for(size_t i = 0; i < 8; ++i) {
            if(addr[i] == 0 && !used_zeros) {
                continue;
            }
            if(i && addr[i - 1] == 0 && !used_zeros) {
                os << ":";
                used_zeros = true;
            }
            if(i) {
                os << ":";
            }
            os << std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
        }

        if(!used_zeros && addr[7] == 0) {
            os << "::";
        }

        os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
        return os;
    }

    uint16_t IPv6Address::get_port() const
    {
        return byteswapOnLittleEndian(m_addr.sin6_port);
    }

    void IPv6Address::set_port(uint16_t port)
    {
        m_addr.sin6_port = byteswapOnLittleEndian(port);
    }

    IPAddress::ptr IPv6Address::broadcast_address(uint32_t prefix_len) const
    {
        sockaddr_in6 addr = m_addr;
        addr.sin6_addr.s6_addr[prefix_len/8] |= create_mask<uint8_t>(prefix_len % 8);
        for (int i = prefix_len / 8 + 1; i < 16; ++i)
        {
            addr.sin6_addr.s6_addr[i] = 0xFF;
        }
        return std::make_shared<IPv6Address>(addr);
    }

    IPAddress::ptr IPv6Address::network_address(uint32_t prefix_len) const
    {
        sockaddr_in6 addr = m_addr;
        addr.sin6_addr.s6_addr[prefix_len/8] &= ~create_mask<uint8_t>(prefix_len % 8);
        for (auto i = prefix_len / 8 + 1; i < 16; ++i)
        {
            addr.sin6_addr.s6_addr[i] = 0;
        }
        return std::make_shared<IPv6Address>(addr);
    }

    IPAddress::ptr IPv6Address::subnet_mask(uint32_t prefix_len) const
    {
        sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = IPV6;
        addr.sin6_addr.s6_addr[prefix_len/8] |= ~create_mask<uint8_t>(prefix_len % 8);
        for (unsigned int i = 0; i < prefix_len / 8; ++i)
        {
            addr.sin6_addr.s6_addr[i] = 0xFF;
        }
        return std::make_shared<IPv6Address>(addr);
    }
    static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;
    UnixAddress::UnixAddress()
    {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
    }

    UnixAddress::UnixAddress(const std::string& path)
    {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length = path.size() + 1;

        if(!path.empty() && path[0] == '\0') {
            --m_length;
        }

        if(m_length > sizeof(m_addr.sun_path)) {
            throw std::logic_error("path too long");
        }
        memcpy(m_addr.sun_path, path.c_str(), m_length);
        m_length += offsetof(sockaddr_un, sun_path);
    }

    const sockaddr* UnixAddress::get_addr() const
    {
        return reinterpret_cast<const sockaddr*>(&m_addr);
    }

    sockaddr* UnixAddress::get_addr()
    {
        return reinterpret_cast<sockaddr*>(&m_addr);
    }

    socklen_t UnixAddress::get_addrlen() const
    {
        return m_length;
    }

    void UnixAddress::set_addrLen(uint32_t v)
    {
        m_length = v;
    }

    std::string UnixAddress::getPath() const
    {
        std::stringstream ss;
        if(m_length > offsetof(sockaddr_un, sun_path)
                && m_addr.sun_path[0] == '\0') {
            ss << "\\0" << std::string(m_addr.sun_path + 1,
                    m_length - offsetof(sockaddr_un, sun_path) - 1);
                } else {
                    ss << m_addr.sun_path;
                }
        return ss.str();
    }

    std::ostream& UnixAddress::insert(std::ostream& os) const
    {
        if(m_length > offsetof(sockaddr_un, sun_path)
            && m_addr.sun_path[0] == '\0') {
            return os << "\\0" << std::string(m_addr.sun_path + 1,
                    m_length - offsetof(sockaddr_un, sun_path) - 1);
            }
        return os << m_addr.sun_path;
    }

    UnknownAddress::UnknownAddress(int family)
    {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sa_family = family;
    }

    UnknownAddress::UnknownAddress(const sockaddr& addr)
    {
        m_addr = addr;
    }

    const sockaddr* UnknownAddress::get_addr() const
    {
        return &m_addr;
    }

    sockaddr* UnknownAddress::get_addr()
    {
        return &m_addr;
    }

    socklen_t UnknownAddress::get_addrlen() const
    {
        return sizeof(m_addr);
    }

    std::ostream& UnknownAddress::insert(std::ostream& os) const
    {
        os << "[UnknownAddress family=" << m_addr.sa_family << "]";
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const Address& addr)
    {
        return addr.insert(os);
    }
}
