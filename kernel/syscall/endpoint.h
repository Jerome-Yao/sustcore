/**
 * @file endpoint.h
 * @brief Endpoint syscalls
 */

#pragma once

#include <sustcore/addr.h>
#include <sustcore/capability.h>
#include <syscall/syscall.h>

#include <cstddef>

namespace syscall {
    bool create_endpoint(CapIdx capidx);
    bool send_msg(CapIdx endpoint, VirAddr msgbuf, size_t msgsz,
                  VirAddr caplist, size_t capsz, bool blocking);
    /**
     * @brief 从端点处接收信息
     *
     * @param endpoint 端点cap索引
     * @param msgbuf 用户缓冲区地址，用于存放接收到的消息
     * @param msgsz 用户缓冲区大小，单位为字节
     * @param caplist 用户缓冲区地址，用于存放接收到的cap索引列表
     * @param capsz 用户缓冲区大小，单位为cap索引个数
     * @return RetPack 返回结果，包含是否成功、是否需要defer、错误码等信息
     */
    util::cotask<RetPack> recv_msg_sync(CapIdx endpoint, VirAddr msgbuf,
                                        VirAddr msgsz, VirAddr caplist,
                                        VirAddr capsz);
    bool recv_msg_async(CapIdx endpoint, VirAddr msgbuf, VirAddr msgsz,
                        VirAddr caplist, VirAddr capsz);
}  // namespace syscall
