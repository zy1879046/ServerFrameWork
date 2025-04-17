//
// Created by 张羽 on 25-4-15.
//

#ifndef UTIL_H
#define UTIL_H
#include <pthread.h>
#include <string>
#include <vector>
namespace sylar{
    pid_t get_thread_id();

    uint64_t get_cur_fiber_id();



    void Backtrace(std::vector<std::string>& bt,int size,int skip = 1);

    std::string BacktraceToString(int size,int skip = 2, const std::string& prefix = " ");

    //获取当前毫秒精度时间
    uint64_t get_cur_ms();
    //获取当前微秒精度时间
    uint64_t get_cur_us();


}
#endif //UTIL_H
