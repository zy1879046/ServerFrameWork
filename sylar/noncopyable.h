//
// Created by 张羽 on 25-4-14.
//

#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H

class NonCopyable
{
public:
    NonCopyable() = default;
    virtual ~NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable& operator=(NonCopyable&&) = delete;
};

#endif //NONCOPYABLE_H
