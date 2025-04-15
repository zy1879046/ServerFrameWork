//
// Created by 张羽 on 25-4-15.
//

#ifndef MACRO_H
#define MACRO_H
#include "util.h"


#define SYLAR_ASSERT(x) \
if(!(x)){  \
LOG_ROOT_ERROR() << "ASSERTION: " << #x \
<< "\nbacktrace\n" << sylar::BacktraceToString(100,2,"   ");\
assert(x); \
}

#define SYLAR_ASSERT2(x,w) \
if(!(x)) { \
LOG_ROOT_ERROR() << "ASSERTION: " << #x \
<<"\n" << w \
<< "\nbacktrace\n" << sylar::BacktraceToString(100,2,"   ");\
}
#endif //MACRO_H
