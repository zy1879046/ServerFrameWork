//
// Created by 张羽 on 25-4-22.
//
#include "socket.h"
#include "log.h"
#include "iomanager.h"

void test_socket()
{
    //获取ip地址
    auto addr = sylar::Address::lookup_any_ip_address("www.baidu.com");
    if (!addr)
    {
        LOG_ROOT_ERROR() << "get ip address failed";
        return;
    }
    //创建与当前地址类型的socket
    auto sock = sylar::Socket::CreateTCP(addr);
    addr->set_port(80);
    if (!sock->connect(addr))
    {
        LOG_ROOT_ERROR() << "connect failed";
        return;
    }
    //链接成功
    LOG_ROOT_INFO() << "connect success";
    //发送数据
    const char buf[] = "GET / HTTP/1.0\r\n\r\n";
    int rt = sock->send(buf, sizeof(buf));
    if (rt <= 0)
    {
        LOG_ROOT_ERROR() << "send failed";
        return;
    }
    LOG_ROOT_INFO() << "send success";
    //接收数据
    std::string buffs;
    buffs.resize(4096);
    rt = sock->recv(&buffs[0], buffs.size());
    if (rt <= 0)
    {
        LOG_ROOT_ERROR() << "recv failed";
        return;
    }
    buffs.resize(rt);
    LOG_ROOT_INFO() << "recv success, size=" << rt;
    LOG_ROOT_INFO() << buffs;

}

void test()
{
    auto addr = sylar::Address::lookup_any_ip_address("www.baidu.com:80");
    if (!addr)
    {
        LOG_ROOT_ERROR() << "get ip address failed";
    }
    auto sock = sylar::Socket::CreateTCP(addr);
    if (!sock->connect(addr))
    {
        LOG_ROOT_ERROR() << "connect failed";
        return;
    }
    LOG_ROOT_INFO() << "connect success";
    uint64_t ts = sylar::get_cur_us();
    for(size_t i = 0; i < 10000000000ul; ++i) {
        if(int err = sock->getError()) {
            LOG_ROOT_ERROR() << "err=" << err << " errstr=" << strerror(err);
            break;
        }

        //struct tcp_info tcp_info;
        //if(!sock->getOption(IPPROTO_TCP, TCP_INFO, tcp_info)) {
        //    SYLAR_LOG_INFO(g_looger) << "err";
        //    break;
        //}
        //if(tcp_info.tcpi_state != TCP_ESTABLISHED) {
        //    SYLAR_LOG_INFO(g_looger)
        //            << " state=" << (int)tcp_info.tcpi_state;
        //    break;
        //}
        static int batch = 10000000;
        if(i && (i % batch) == 0) {
            uint64_t ts2 = sylar::get_cur_us();
            LOG_ROOT_ERROR() << "i=" << i << " used: " << ((ts2 - ts) * 1.0 / batch) << " us";
            ts = ts2;
        }
    }
}
int main()
{
    sylar::IOManager iom;
    iom.schedule(test_socket);
    // iom.schedule(test);
    return 0;

}