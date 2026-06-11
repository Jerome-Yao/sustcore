/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 文件系统测试 A
 * @version alpha-1.0.0
 * @date 2026-06-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    constexpr const char *TMPFS_DIR0  = "/tmpfs/";
    constexpr const char *TMPFS_DIR1  = "/tmpfs/abc/";
    constexpr const char *TMPFS_FILE  = "/tmpfs/abc/file1.txt";
    constexpr const char *MODULE_B    = "/initrd/test_file_rw_b.mod";
    constexpr const char *TEST_TEXT =
        "Hello, TmpFS! This is the test text from test_file_rw_a!\n"
        "你好TmpFS, 这是来自 test_file_rw_a 进程的测试文本!";
}

int kmod_main() {
    printf("test_file_rw_a: start pid=%u\n", sys_getpid(__pcb_cap));

    if (kmod_mkdir(TMPFS_DIR0) != 0) {
        printf("test_file_rw_a: mkdir failed: %s\n", TMPFS_DIR0);
        exit(-1);
    }
    printf("test_file_rw_a: mkdir ok: %s\n", TMPFS_DIR0);

    if (kmod_mkdir(TMPFS_DIR1) != 0) {
        printf("test_file_rw_a: mkdir failed: %s\n", TMPFS_DIR1);
        exit(-1);
    }
    printf("test_file_rw_a: mkdir ok: %s\n", TMPFS_DIR1);

    int fd = kmod_mkfile(TMPFS_FILE, "w+");
    if (fd < 0) {
        printf("test_file_rw_a: mkfile failed: %s\n", TMPFS_FILE);
        exit(-1);
    }
    printf("test_file_rw_a: mkfile ok: %s\n", TMPFS_FILE);

    const size_t text_len = strlen(TEST_TEXT);
    size_t written        = kmod_fwrite(fd, TEST_TEXT, text_len);
    if (written != text_len) {
        printf("test_file_rw_a: fwrite failed, expect=%u actual=%u\n",
               static_cast<unsigned>(text_len), static_cast<unsigned>(written));
        kmod_fclose(fd);
        exit(-1);
    }
    kmod_fclose(fd);
    printf("test_file_rw_a: write ok, len=%u\n",
           static_cast<unsigned>(text_len));

    int exec_fd = kmod_fopen(MODULE_B, "x");
    if (exec_fd < 0) {
        printf("test_file_rw_a: open module B failed: %s\n", MODULE_B);
        exit(-1);
    }

    printf("test_file_rw_a: execve -> %s\n", MODULE_B);
    if (!execve(kmod_getcap(exec_fd), nullptr, 0, nullptr, 0)) {
        printf("test_file_rw_a: execve failed\n");
        kmod_fclose(exec_fd);
        exit(-1);
    }

    kmod_fclose(exec_fd);
    printf("test_file_rw_a: unexpected return after execve\n");
    exit(-1);
    return 0;
}
