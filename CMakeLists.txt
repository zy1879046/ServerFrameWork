cmake_minimum_required(VERSION 2.8)

project(sylar)
include (cmake/utils.cmake)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS}  -g -D_GNU_SOURCE -D_XOPEN_SOURCE -std=c++17 -Wall  -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined ")
# 设置 C++ 标准为 C++11
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 查找 boost 库
find_package(Boost REQUIRED)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
else ()
    message(FATAL_ERROR "Boost not found")
endif ()
# 查找 yaml-cpp 库
find_package(yaml-cpp REQUIRED)
if (yaml-cpp_FOUND)
    include_directories(${YAML_CPP_INCLUDE_DIR})
    message(STATUS "yaml-cpp_INCLUDE_DIRS: ${YAML_CPP_INCLUDE_DIR}")
else ()
    message(FATAL_ERROR "yaml-cpp not found")
endif ()
include_directories(sylar)
set(LIB_SRC
    sylar/log.h
    sylar/log.cpp
    sylar/Singleton.h
    sylar/config.h
    sylar/config.cpp
        sylar/noncopyable.h
        sylar/mutex.h
        sylar/mutex.cpp
        sylar/Thread.cpp
        sylar/Thread.h
        sylar/util.h
        sylar/util.cpp
        sylar/macro.h
        sylar/fiber.cpp
        sylar/fiber.h
        sylar/schedule.cpp
        sylar/schedule.h
        sylar/iomanager.h
        sylar/iomanager.cpp
        sylar/timer.h
        sylar/timer.cpp
        sylar/hook.h
        sylar/hook.cpp
        sylar/fd_manager.h
        sylar/fd_manager.cpp
        sylar/address.h
        sylar/address.cpp
        sylar/myendian.h
        sylar/socket.h
        sylar/socket.cpp
)

set(LIB_LIB
    sylar
    pthread
    ${Boost_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
)
add_library(sylar SHARED ${LIB_SRC})

force_redefine_file_macro_for_sources(test) #__FILE__ 重定义该宏为相对路径
force_redefine_file_macro_for_sources(test_log) #__FILE__ 重定义该宏为相对路径
force_redefine_file_macro_for_sources(sylar) #__FILE__ 重定义该宏为相对路径


add_executable(test_log test/test_log.cpp)
add_dependencies(test_log sylar)
target_link_libraries(test_log ${LIB_LIB})

add_executable(test_config test/test_config.cpp)
add_dependencies(test_config sylar)
force_redefine_file_macro_for_sources(test_config)
target_link_libraries(test_config ${LIB_LIB})

add_executable(test_thread test/test_thread.cpp)
add_dependencies(test_thread sylar)
force_redefine_file_macro_for_sources(test_thread) #__FILE__ 重定义该宏为相对路径
target_link_libraries(test_thread ${LIB_LIB})

add_executable(test_fiber test/test_fiber.cpp)
add_dependencies(test_fiber sylar)
force_redefine_file_macro_for_sources(test_fiber) #__FILE__ 重定义该宏为相对路径
target_link_libraries(test_fiber ${LIB_LIB})

add_executable(test_schedule test/test_schedule.cpp)
add_dependencies(test_schedule sylar)
force_redefine_file_macro_for_sources(test_schedule) #__FILE__ 重定义该宏为相对路径
target_link_libraries(test_schedule ${LIB_LIB})

add_executable(test_iomanager test/test_iomanager.cpp)
add_dependencies(test_iomanager sylar)
force_redefine_file_macro_for_sources(test_iomanager) #__FILE__ 重定义该宏为相对路径
target_link_libraries(test_iomanager ${LIB_LIB})


add_executable(test_hook test/test_hook.cpp)
add_dependencies(test_hook sylar)
force_redefine_file_macro_for_sources(test_hook) #__FILE__ 重定义该宏为相对路径
target_link_libraries(test_hook ${LIB_LIB})

add_executable(test_address test/test_address.cpp
        )
add_dependencies(test_address sylar)
force_redefine_file_macro_for_sources(test_address) #__FILE__ 重定义该宏为相对路径
target_link_libraries(test_address ${LIB_LIB})

add_executable(test_socket test/test_socket.cpp)
add_dependencies(test_socket sylar)
force_redefine_file_macro_for_sources(test_socket) #__FILE__ 重定义该宏为相对路径
target_link_libraries(test_socket ${LIB_LIB})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

