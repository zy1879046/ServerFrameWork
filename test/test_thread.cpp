//
// Created by 张羽 on 25-4-15.
//
/* ************************************************************************
> File Name:     test_thread.cpp
> Author:        程序员XiaoJiu
> mail           zy18790460359@gmail.com
> Created Time:  四  5/16 21:23:09 2024
> Description:   
 ************************************************************************/

 #include "config.h"
#include "Thread.h"
#include "log.h"
 #include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "util.h"


int count = 0;
sylar::Mutex s_mutex;
 
void func1(){
 
    for(int i = 0; i < 100000; ++i){
        sylar::LockGuard<sylar::Mutex> lock(s_mutex);
        ++count;
    }
 
    LOG_ROOT_INFO() << "name: " <<sylar::Thread::GetThis()->get_name()
                             << " id: " << sylar::get_thread_id()
                             <<" this.id: " << sylar::Thread::GetThis()->get_pid();
}
void func2(){
    auto i = 100;
    while(--i){
        LOG_ROOT_INFO() << "####################################";
    }
}

void func3(){
    auto i = 100;
    while(--i){
        LOG_ROOT_INFO() << "=====================================";
    }
}
void test_condition() {
    sylar::Mutex s_mutex;
    sylar::ConditionVariable cond;

    int i = 0;
    bool running = true;

    sylar::Thread t1([&i, &cond, &s_mutex, &running] {
        while (running) {
            sylar::UniqueLock<sylar::Mutex> lock(s_mutex);
            cond.wait(lock, [&i] { return (i % 2) == 0; });
            LOG_ROOT_INFO() << "i: " << i;
            ++i;
            cond.notify();
        }
    });

    sylar::Thread t2([&i, &cond, &s_mutex, &running] {
        while (running) {
            sylar::UniqueLock<sylar::Mutex> lock(s_mutex);
            cond.wait(lock, [&i] { return (i % 2) == 1; });
            LOG_ROOT_INFO() << "i: " << i;
            ++i;
            cond.notify();
        }
    });

    // 模拟运行一段时间后退出
    std::this_thread::sleep_for(std::chrono::seconds(10));
    running = false;

    t1.join();
    t2.join();
}
int main(int argc , char* argv[]){
    LOG_ROOT_INFO() << "thread test begin";
    // YAML::Node root = YAML::LoadFile("../bin/config/log.yml");
    // CONFIG_ENV()->load_from_yaml(root);
    // std::vector<sylar::Thread::ptr> thrs;
    // for(int i = 0; i < 2 ;++i){
    //     sylar::Thread::ptr thr(new sylar::Thread(func2,"name_" + std::to_string(i)));
    //     sylar::Thread::ptr thr2(new sylar::Thread(func3,"name_" + std::to_string(i)));
    //
    //     thrs.emplace_back(thr2);
    //     thrs.emplace_back(thr);
    // }
    // for(auto i = 0u; i < thrs.size(); ++i){
    //     thrs[i]->join();
    // }
    // func2();
    test_condition();
    LOG_ROOT_INFO() << "thread test end";
    LOG_ROOT_INFO() << "count = " << count;
    return 0;
 
}
 