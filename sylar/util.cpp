//
// Created by 张羽 on 25-4-15.
//
#include "util.h"
#include <unistd.h>
#include <sys/syscall.h>
namespace sylar
{
    static thread_local pid_t t_id = 0;
    pid_t get_thread_id()
    {
        if(t_id != 0)return t_id;
        // 使用系统调用获取线程 ID
        t_id = static_cast<pid_t>(::syscall(SYS_gettid));
        return t_id;
    }
}