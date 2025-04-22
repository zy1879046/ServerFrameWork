//
// Created by 张羽 on 25-4-21.
//
#include "hook.h"
#include "log.h"
#include "iomanager.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>



void test_sleep() {
    sylar::IOManager iom(1);
    iom.schedule([](){
        sleep(2);
        LOG_ROOT_INFO() << "sleep 2";
    });

    iom.schedule([](){
        sleep(3);
        LOG_ROOT_INFO() << "sleep 3";
    });
    LOG_ROOT_INFO() << "test_sleep";
}

void test_sock() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "112.80.248.75", &addr.sin_addr.s_addr);

    LOG_ROOT_INFO() << "begin connect";
    int rt = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    LOG_ROOT_INFO() << "connect rt=" << rt << " errno=" << errno;

    if(rt) {
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    LOG_ROOT_INFO() << "send rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    std::string buff;
    buff.resize(4096);

    rt = recv(sock, &buff[0], buff.size(), 0);
    LOG_ROOT_INFO() << "recv rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    buff.resize(rt);
    LOG_ROOT_INFO() << buff;
}

int main(int argc, char** argv) {
    test_sleep();
    sylar::IOManager iom;
    iom.schedule(test_sock);
    return 0;
}
