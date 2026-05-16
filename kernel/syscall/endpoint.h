/**
 * @file endpoint.h
 * @brief Endpoint syscalls
 */

#pragma once

#include <cstddef>
#include <sustcore/addr.h>
#include <sustcore/capability.h>

namespace syscall {
    bool create_endpoint(CapIdx capidx);
    bool send_msg(CapIdx endpoint, VirAddr msgbuf, size_t msgsz,
                  VirAddr caplist, size_t capsz, bool blocking);
    bool recv_msg_sync(CapIdx endpoint, VirAddr msgbuf, VirAddr msgsz,
                       VirAddr caplist, VirAddr capsz);
    bool recv_msg_async(CapIdx endpoint, VirAddr msgbuf, VirAddr msgsz,
                        VirAddr caplist, VirAddr capsz);
}  // namespace syscall
