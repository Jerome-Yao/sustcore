/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 主文件
 * @version alpha-1.0.0
 * @date 2026-04-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kmod/syscall.h>

#include <cstdio>

int kmod_main() {
    printf("进入 init 模块!\n");

    int fd = kmod_fopen("/initrd/test_fork.mod", "x");
    if (fd >= 0) {
        sys_create_process(kmod_getcap(fd), nullptr, 0, SCHED_CLASS_RR);
        kmod_fclose(fd);
    }

    fd = kmod_fopen("/initrd/test_thread.mod", "x");
    if (fd >= 0) {
        sys_create_process(kmod_getcap(fd), nullptr, 0, SCHED_CLASS_RR);
        kmod_fclose(fd);
    }

    fd = kmod_fopen("/initrd/test_endpoint_master.mod", "x");
    if (fd >= 0) {
        sys_create_process(kmod_getcap(fd), nullptr, 0, SCHED_CLASS_RR);
        kmod_fclose(fd);
    }

    fd = kmod_fopen("/initrd/test_call_service.mod", "x");
    if (fd >= 0) {
        sys_create_process(kmod_getcap(fd), nullptr, 0, SCHED_CLASS_RR);
        kmod_fclose(fd);
    }

    fd = kmod_fopen("/initrd/test_rpc_server.mod", "x");
    if (fd >= 0) {
        sys_create_process(kmod_getcap(fd), nullptr, 0, SCHED_CLASS_RR);
        kmod_fclose(fd);
    }

    printf("init: 启动完成, 退出\n");
    exit(0);
    return 0;
}
