//
// Created by 张羽 on 25-4-15.
//
#include "util.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <execinfo.h>
#include "log.h"
#include "fiber.h"
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

    uint64_t get_cur_fiber_id()
    {
        // LOG_ROOT_DEBUG() << "get_cur_fiber_id";
        return Fiber::get_cur_fiber_id();
    }
    void Backtrace(std::vector<std::string>& bt,int size,int skip){
        void** array = (void**)malloc(sizeof(void*) * size);
        //通过该函数获取调用栈，并将调用栈信息存在array中，返回栈中的大小
        size_t s = ::backtrace(array,size);
        //将当前调用栈转化为 string类型
        char** strings = backtrace_symbols(array,s);
        if(strings == NULL){
            LOG_ROOT_ERROR() << "backtrace_symbols error";
            return;
        }
        for(size_t i = skip; i < s ; ++i){
            bt.emplace_back(strings[i]);
        }
        free(strings);
        free(array);
    }
    std::string BacktraceToString(int size,int skip, const std::string& prefix){
        std::vector<std::string> bt;
        Backtrace(bt,size,skip);
        std::stringstream ss;
        for(size_t i = 0; i < bt.size() ; ++i){
            ss << prefix << bt[i] << std::endl;
        }
        return ss.str();
    }
}