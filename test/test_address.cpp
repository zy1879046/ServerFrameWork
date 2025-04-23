//
// Created by 张羽 on 25-4-22.
//
#include "address.h"
#include "log.h"

//sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test() {
    std::vector<sylar::Address::ptr> addrs;

    LOG_ROOT_INFO() << "begin";
    // bool v = sylar::Address::lookup(addrs, "localhost:3080");
    bool v = sylar::Address::lookup(addrs, "www.baidu.com", AF_INET);
    //bool v = sylar::Address::Lookup(addrs, "www.sylar.top", AF_INET);
    LOG_ROOT_INFO() << "end";
    if(!v) {
        LOG_ROOT_ERROR() << "lookup fail";
        return;
    }

    for(size_t i = 0; i < addrs.size(); ++i) {
        LOG_ROOT_INFO() << i << " - " << addrs[i]->to_string();
    }

    auto addr = sylar::Address::lookup_any("localhost:4080");
    if(addr) {
        LOG_ROOT_INFO() << *addr;
    } else {
        LOG_ROOT_ERROR() << "error";
    }
}

void test_iface() {
    std::multimap<std::string, std::pair<sylar::Address::ptr, uint32_t> > results;

    bool v = sylar::Address::get_interface_address(results);
    if(!v) {
        LOG_ROOT_ERROR() << "GetInterfaceAddresses fail";
        return;
    }

    for(auto& i: results) {
        LOG_ROOT_INFO() << i.first << " - " << i.second.first->to_string() << " - "
            << i.second.second;
    }
}

void test_ipv4() {
    // auto addr = sylar::IPAddress::create("www.sylar.top");
    auto addr = sylar::IPAddress::create("127.0.0.8");
    auto ipv4 = sylar::Address::lookup_any("www.baidu.com", AF_INET);
    if(addr) {
        LOG_ROOT_INFO() << addr->to_string();
        LOG_ROOT_INFO() << ipv4->to_string();
    }
}

int main(int argc, char** argv) {
    test_ipv4();
    // test_iface();
    // test();
    return 0;
}
