//
// Created by 张羽 on 25-4-16.
//
#include <unistd.h>

#include "schedule.h"



void test_fiber() {
    static int s_count = 5;
    LOG_ROOT_INFO() << "test in fiber s_count = " << s_count;
    
    sleep(1);
    if(-- s_count >= 0) {
        // 未指定线程ID，表示任意线程都能执行任务
        // sylar::Schedule::get_this()->schedule(&test_fiber,sylar::get_thread_id());
        sylar::Schedule::get_this()->schedule(&test_fiber);
    }
    
}

int main(int argc, char** argv) {
    LOG_ROOT_INFO() << "main";
    sylar::Thread::set_name("main");
    sylar::Schedule sc(2,true, "test");
    sleep(2);
    sc.start();
    sc.schedule(&test_fiber);
    sc.stop();
    LOG_ROOT_INFO() << "over";
    
    return 0;
}

