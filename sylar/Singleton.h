//
// Created by 张羽 on 25-4-10.
//

#ifndef SINGLETON_H
#define SINGLETON_H
#include <memory>
#include <mutex>
#include <atomic>
template<typename T>
class Singleton
{
public:
    Singleton(Singleton const&) = delete;
    Singleton& operator=(Singleton const&) = delete;
    virtual ~Singleton() = default;
    static std::shared_ptr<T> GetInstance()
    {
        static std::once_flag flag;
        std::call_once(flag, []() {
            instance.reset(new T());
        });
        return instance;
    }

protected:
    Singleton() = default;
    static std::shared_ptr<T> instance;
};
template<typename T>
std::shared_ptr<T> Singleton<T>::instance = nullptr;
#endif //SINGLETON_H
