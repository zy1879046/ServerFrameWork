//
// Created by 张羽 on 25-4-22.
//

#ifndef MY_ENDIAN_H
#define MY_ENDIAN_H

#define SYLAR_LITTLE_ENDIAN 1
#define SYLAR_BIG_ENDIAN 2
#include <type_traits>
#include <byteswap.h>
#include <cstdint>
#include <sys/types.h>
//#include <endian.h>

// #ifdef __cplusplus
// extern "C++" {
// #endif
namespace sylar {

    /**
     * @brief 8字节类型的字节序转化
     */
    template<typename T>
    typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
    byteswap(T value) {
        return (T)bswap_64((uint64_t)value);
    }

    /**
     * @brief 4字节类型的字节序转化
     */
    template<typename T>
    typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
    byteswap(T value) {
        return (T)bswap_32((uint32_t)value);
    }

    /**
     * @brief 2字节类型的字节序转化
     */
    template<typename T>
    typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
    byteswap(T value) {
        return (T)bswap_16((uint16_t)value);
    }

#if BYTE_ORDER == BIG_ENDIAN
#define SYLAR_BYTE_ORDER SYLAR_BIG_ENDIAN
#else
#define SYLAR_BYTE_ORDER SYLAR_LITTLE_ENDIAN
#endif

#if SYLAR_BYTE_ORDER == SYLAR_BIG_ENDIAN

    /**
     * @brief 只在小端机器上执行byteswap, 在大端机器上什么都不做
     */
    template<typename T>
    T byteswapOnLittleEndian(T t) {
        return t;
    }

    /**
     * @brief 只在大端机器上执行byteswap, 在小端机器上什么都不做
     */
    template<typename T>
    T byteswapOnBigEndian(T t) {
        return byteswap(t);
    }
#else

    /**
     * @brief 只在小端机器上执行byteswap, 在大端机器上什么都不做
     */
    template<typename T>
    T byteswapOnLittleEndian(T t) {
        return byteswap(t);
    }

    /**
     * @brief 只在大端机器上执行byteswap, 在小端机器上什么都不做
     */
    template<typename T>
    T byteswapOnBigEndian(T t) {
        return t;
    }
#endif

}
// #ifdef __cplusplus
// }
// #endif
#endif //ENDIAN_H
