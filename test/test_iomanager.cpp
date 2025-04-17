//
// Created by 张羽 on 25-4-17.
//

#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "iomanager.h"

int sock = 0;

void test_fiber() {
    LOG_ROOT_INFO() << "test_fiber";

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "112.80.248.75", &addr.sin_addr.s_addr);

    if (!connect(sock, (const sockaddr*)&addr, sizeof(addr))) {
    } else if(errno == EINPROGRESS) {
        LOG_ROOT_INFO() << "add event errno=" << errno << " " << strerror(errno);

        sylar::IOManager::get_this()->add_event(sock, sylar::IOManager::Read, [](){
            LOG_ROOT_INFO() << "read callback";
            char temp[1000];
            int rt = read(sock, temp, 1000);
            if (rt >= 0) {
                std::string ans(temp, rt);
                LOG_ROOT_INFO() << "read:["<< ans << "]";
            } else {
                LOG_ROOT_INFO() << "read rt = " << rt;
            }
            });
        sylar::IOManager::get_this()->add_event(sock, sylar::IOManager::Write, [](){
            LOG_ROOT_INFO() << "write callback";
            int rt = write(sock, "GET / HTTP/1.1\r\ncontent-length: 0\r\n\r\n",38);
            LOG_ROOT_INFO() << "write rt = " << rt;
            });
    } else {
        LOG_ROOT_INFO() << "else " << errno << " " << strerror(errno);
    }
}

void test01() {
    sylar::IOManager iom(2, true, "IOM");
    iom.schedule(test_fiber);
}

int main(int argc, char** argv) {
    // g_logger->setLevel(sylar::LogLevel::INFO);
    test01();

    return 0;
}

