//
// Created by 张羽 on 25-4-15.
//
#include "log.h"
#include "fiber.h"
#include "Thread.h"
#include <string>


void run_in_fiber(){
    LOG_ROOT_INFO() << "run_in_fiber begin";
    sylar::Fiber::yied_to_hold();
    LOG_ROOT_INFO() << "run_in_fiber end";
    // sylar::Fiber::yied_to_hold();
}
void test_fiber(){
    LOG_ROOT_INFO() << "main begin -1";
    {
        sylar::Fiber::GetCurFiber();
        LOG_ROOT_INFO() << "main begin";
        sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));
        //LOG_ROOT_INFO() << "a test ";
        fiber->swap_in();
        LOG_ROOT_INFO() << "main after swapIn";
        fiber->swap_in();
        LOG_ROOT_INFO() << "main after end";
    }
    LOG_ROOT_INFO() << "main after end 2";
    LOG_ROOT_INFO() << "fiber_num: "<<sylar::Fiber::get_total_fibers();
}
int main(int argc, char* argv[]){
    // test_fiber();
    sylar::Thread::set_name("main");
    std::vector<sylar::Thread::ptr> thrs;
    for(size_t i = 0; i < 3; ++i){
        thrs.push_back(sylar::Thread::ptr(new sylar::Thread(&test_fiber,"name_"+std::to_string(i))));
    }
    for(auto& i : thrs){
        i->join();
    }
    LOG_ROOT_INFO() << "fiber_num: "<<sylar::Fiber::get_total_fibers();
    return 0;
}
