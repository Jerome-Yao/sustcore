/**
 * @file clone.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief clone 系统调用支持
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <errno.h>
#include <prog.h>
#include <std/stdio.h>
#include <sus/types.h>
#include <thread.h>

#include <cstddef>
#include <string>
#include <syscall.h.in>

namespace {
    struct CloneFlagName {
        size_t value;
        const char *name;
    };

    constexpr size_t CSIGNAL             = 0x000000ff;
    constexpr size_t CLONE_VM            = 0x00000100;
    constexpr size_t CLONE_FS            = 0x00000200;
    constexpr size_t CLONE_FILES         = 0x00000400;
    constexpr size_t CLONE_SIGHAND       = 0x00000800;
    constexpr size_t CLONE_PIDFD         = 0x00001000;
    constexpr size_t CLONE_PTRACE        = 0x00002000;
    constexpr size_t CLONE_VFORK         = 0x00004000;
    constexpr size_t CLONE_PARENT        = 0x00008000;
    constexpr size_t CLONE_THREAD        = 0x00010000;
    constexpr size_t CLONE_NEWNS         = 0x00020000;
    constexpr size_t CLONE_SYSVSEM       = 0x00040000;
    constexpr size_t CLONE_SETTLS        = 0x00080000;
    constexpr size_t CLONE_PARENT_SETTID = 0x00100000;
    constexpr size_t CLONE_CHILD_CLEARTID = 0x00200000;
    constexpr size_t CLONE_DETACHED      = 0x00400000;
    constexpr size_t CLONE_UNTRACED      = 0x00800000;
    constexpr size_t CLONE_CHILD_SETTID  = 0x01000000;
    constexpr size_t CLONE_NEWCGROUP     = 0x02000000;
    constexpr size_t CLONE_NEWUTS        = 0x04000000;
    constexpr size_t CLONE_NEWIPC        = 0x08000000;
    constexpr size_t CLONE_NEWUSER       = 0x10000000;
    constexpr size_t CLONE_NEWPID        = 0x20000000;
    constexpr size_t CLONE_NEWNET        = 0x40000000;
    constexpr size_t CLONE_IO            = 0x80000000;
    constexpr size_t CLONE_NEWTIME       = 0x00000080;

    constexpr CloneFlagName CLONE_FLAG_NAMES[] = {
        {.value = CLONE_NEWTIME, .name = "CLONE_NEWTIME"},
        {.value = CLONE_VM, .name = "CLONE_VM"},
        {.value = CLONE_FS, .name = "CLONE_FS"},
        {.value = CLONE_FILES, .name = "CLONE_FILES"},
        {.value = CLONE_SIGHAND, .name = "CLONE_SIGHAND"},
        {.value = CLONE_PIDFD, .name = "CLONE_PIDFD"},
        {.value = CLONE_PTRACE, .name = "CLONE_PTRACE"},
        {.value = CLONE_VFORK, .name = "CLONE_VFORK"},
        {.value = CLONE_PARENT, .name = "CLONE_PARENT"},
        {.value = CLONE_THREAD, .name = "CLONE_THREAD"},
        {.value = CLONE_NEWNS, .name = "CLONE_NEWNS"},
        {.value = CLONE_SYSVSEM, .name = "CLONE_SYSVSEM"},
        {.value = CLONE_SETTLS, .name = "CLONE_SETTLS"},
        {.value = CLONE_PARENT_SETTID, .name = "CLONE_PARENT_SETTID"},
        {.value = CLONE_CHILD_CLEARTID, .name = "CLONE_CHILD_CLEARTID"},
        {.value = CLONE_DETACHED, .name = "CLONE_DETACHED"},
        {.value = CLONE_UNTRACED, .name = "CLONE_UNTRACED"},
        {.value = CLONE_CHILD_SETTID, .name = "CLONE_CHILD_SETTID"},
        {.value = CLONE_NEWCGROUP, .name = "CLONE_NEWCGROUP"},
        {.value = CLONE_NEWUTS, .name = "CLONE_NEWUTS"},
        {.value = CLONE_NEWIPC, .name = "CLONE_NEWIPC"},
        {.value = CLONE_NEWUSER, .name = "CLONE_NEWUSER"},
        {.value = CLONE_NEWPID, .name = "CLONE_NEWPID"},
        {.value = CLONE_NEWNET, .name = "CLONE_NEWNET"},
        {.value = CLONE_IO, .name = "CLONE_IO"},
    };

    [[nodiscard]]
    std::string append_hex(size_t value) {
        char buf[32]{};
        snprintf(buf, sizeof(buf), "0x%016lx",
                 static_cast<unsigned long>(value));
        return std::string(buf);
    }

    [[nodiscard]]
    std::string clone_flags_to_string(size_t flags) {
        std::string out{};
        size_t remaining = flags;

        for (const auto &flag_name : CLONE_FLAG_NAMES) {
            if ((flags & flag_name.value) == 0) {
                continue;
            }
            if (!out.empty()) {
                out += " | ";
            }
            out       += flag_name.name;
            remaining &= ~flag_name.value;
        }

        if (remaining != 0) {
            if (!out.empty()) {
                out += " | ";
            }
            out += append_hex(remaining);
        }

        if (out.empty()) {
            out = "0";
        }
        return out;
    }

    [[nodiscard]]
    bool thread_like_clone(size_t flags) noexcept {
        constexpr size_t CLONE_VM_FLAG     = 0x00000100;
        constexpr size_t CLONE_THREAD_FLAG = 0x00010000;
        return (flags & CLONE_VM_FLAG) != 0 && (flags & CLONE_THREAD_FLAG) != 0;
    }
}  // namespace

CapIdx clone_process(size_t flags, addr_t newsp, int *parent_tid,
                     int *child_tid, addr_t tls) {
    printf(
        "linux-subsystem: clone_process placeholder flags=0x%016lx newsp=%p "
        "parent_tid=%p child_tid=%p tls=%p\n",
        static_cast<unsigned long>(flags), reinterpret_cast<void *>(newsp),
        parent_tid, child_tid, reinterpret_cast<void *>(tls));
    return cap::error;
}

CapIdx clone_thread(size_t flags, addr_t newsp, int *parent_tid, int *child_tid,
                    addr_t tls) {
    printf(
        "linux-subsystem: clone_thread placeholder flags=0x%016lx newsp=%p "
        "parent_tid=%p child_tid=%p tls=%p\n",
        static_cast<unsigned long>(flags), reinterpret_cast<void *>(newsp),
        parent_tid, child_tid, reinterpret_cast<void *>(tls));
    return cap::error;
}

size_t linux_sys_clone(size_t flags, addr_t newsp, int *parent_tid,
                       int *child_tid, addr_t tls) {
    auto flags_str = clone_flags_to_string(flags);
    size_t csignal = flags & CSIGNAL;
    printf(
        "linux-subsystem: clone flags=0x%016lx (%s) newsp=%p parent_tid=%p "
        "child_tid=%p tls=%p CSIGNAL=0x%02lx\n",
        static_cast<unsigned long>(flags), flags_str.c_str(),
        reinterpret_cast<void *>(newsp), parent_tid, child_tid,
        reinterpret_cast<void *>(tls), static_cast<unsigned long>(csignal));

    if (thread_like_clone(flags)) {
        printf("linux-subsystem: clone dispatch => clone_thread\n");
    } else {
        printf("linux-subsystem: clone dispatch => clone_process\n");
    }

    linux_sys_exit(-1);
    return static_cast<size_t>(-ENOSYS);
}
