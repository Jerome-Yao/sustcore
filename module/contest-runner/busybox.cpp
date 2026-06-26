/**
 * @file busybox.cpp
 * @author theflysong
 * @brief busybox 测试运行逻辑
 * @version alpha-1.0.0
 * @date 2026-06-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cstdio>
#include <cstring>

#include "runner.h"

namespace contest_runner {
    namespace {
        constexpr size_t PATH_BUFFER_SIZE = 256;
        constexpr size_t COPY_BUFFER_SIZE = 256;

        enum class BusyboxCaseKind {
            SIMPLE,
            WRITE_FILE,
            APPEND_FILE,
            SORT_UNIQ_FILE,
            CHECK_FILE_EXISTS,
            BACKGROUND_SLEEP_KILL,
        };

        struct BusyboxCase {
            const char *display;
            BusyboxCaseKind kind;
            const char *argv[8];
            const char *file_path;
            const char *file_content;
        };

        constexpr BusyboxCase BUSYBOX_CASES[] = {
            {.display   = "echo \"#### independent command test\"",
             .kind      = BusyboxCaseKind::SIMPLE,
             .argv      = {"busybox", "echo", "#### independent command test",
                           nullptr},
             .file_path = nullptr,
             .file_content = nullptr},
            {.display      = "ash -c exit",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "ash", "-c", "exit", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "sh -c exit",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "sh", "-c", "exit", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "basename /aaa/bbb",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "basename", "/aaa/bbb", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "cal",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "cal", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "clear",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "clear", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "date",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "date", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "df",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "df", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "dirname /aaa/bbb",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "dirname", "/aaa/bbb", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "dmesg",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "dmesg", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "du",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "du", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "expr 1 + 1",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "expr", "1", "+", "1", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "false",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "false", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "true",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "true", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "which ls",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "which", "ls", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "uname",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "uname", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "uptime",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "uptime", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = R"(printf "abc\n")",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "printf", "abc\n", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "ps",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "ps", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "pwd",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "pwd", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "free",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "free", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "hwclock",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "hwclock", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "sh -c 'sleep 5' & ./busybox kill $!",
             .kind         = BusyboxCaseKind::BACKGROUND_SLEEP_KILL,
             .argv         = {nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "ls",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "ls", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "sleep 1",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "sleep", "1", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display = "echo \"#### file opration test\"",
             .kind    = BusyboxCaseKind::SIMPLE,
             .argv    = {"busybox", "echo", "#### file opration test", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "touch test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "touch", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "echo \"hello world\" > test.txt",
             .kind         = BusyboxCaseKind::WRITE_FILE,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = "hello world\n"},
            {.display      = "cat test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "cat", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "cut -c 3 test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "cut", "-c", "3", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "od test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "od", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "head test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "head", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "tail test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "tail", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "hexdump -C test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "hexdump", "-C", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "md5sum test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "md5sum", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "echo \"ccccccc\" >> test.txt",
             .kind         = BusyboxCaseKind::APPEND_FILE,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = "ccccccc\n"},
            {.display      = "echo \"bbbbbbb\" >> test.txt",
             .kind         = BusyboxCaseKind::APPEND_FILE,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = "bbbbbbb\n"},
            {.display      = "echo \"aaaaaaa\" >> test.txt",
             .kind         = BusyboxCaseKind::APPEND_FILE,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = "aaaaaaa\n"},
            {.display      = "echo \"2222222\" >> test.txt",
             .kind         = BusyboxCaseKind::APPEND_FILE,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = "2222222\n"},
            {.display      = "echo \"1111111\" >> test.txt",
             .kind         = BusyboxCaseKind::APPEND_FILE,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = "1111111\n"},
            {.display      = "echo \"bbbbbbb\" >> test.txt",
             .kind         = BusyboxCaseKind::APPEND_FILE,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = "bbbbbbb\n"},
            {.display      = "sort test.txt | ./busybox uniq",
             .kind         = BusyboxCaseKind::SORT_UNIQ_FILE,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = nullptr},
            {.display      = "stat test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "stat", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "strings test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "strings", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "wc test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "wc", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "[ -f test.txt ]",
             .kind         = BusyboxCaseKind::CHECK_FILE_EXISTS,
             .argv         = {nullptr},
             .file_path    = "test.txt",
             .file_content = nullptr},
            {.display      = "more test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "more", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "rm test.txt",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "rm", "test.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "mkdir test_dir",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "mkdir", "test_dir", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "mv test_dir test",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "mv", "test_dir", "test", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "rmdir test",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "rmdir", "test", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display = "grep hello busybox_cmd.txt",
             .kind    = BusyboxCaseKind::SIMPLE,
             .argv = {"busybox", "grep", "hello", "busybox_cmd.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display = "cp busybox_cmd.txt busybox_cmd.bak",
             .kind    = BusyboxCaseKind::SIMPLE,
             .argv    = {"busybox", "cp", "busybox_cmd.txt", "busybox_cmd.bak",
                         nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display      = "rm busybox_cmd.bak",
             .kind         = BusyboxCaseKind::SIMPLE,
             .argv         = {"busybox", "rm", "busybox_cmd.bak", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
            {.display = "find -name \"busybox_cmd.txt\"",
             .kind    = BusyboxCaseKind::SIMPLE,
             .argv = {"busybox", "find", "-name", "busybox_cmd.txt", nullptr},
             .file_path    = nullptr,
             .file_content = nullptr},
        };

        [[nodiscard]]
        bool make_path(char *buf, size_t bufsiz, const char *lhs,
                       const char *rhs) {
            if (buf == nullptr || bufsiz == 0 || lhs == nullptr ||
                rhs == nullptr)
            {
                return false;
            }
            int len = snprintf(buf, bufsiz, "%s/%s", lhs, rhs);
            return len > 0 && static_cast<size_t>(len) < bufsiz;
        }

        [[nodiscard]]
        bool write_file_content(const char *path, const char *content,
                                bool append) {
            if (path == nullptr || content == nullptr) {
                return false;
            }

            if (!append && kmod_truncate(path, 0) < 0) {
                int mkfd = kmod_mkfile(path, "w");
                if (mkfd >= 0) {
                    kmod_fclose(mkfd);
                }
            }

            int fd = append ? kmod_fopen(path, "a") : kmod_fopen(path, "w");
            if (fd < 0 && !append) {
                fd = kmod_mkfile(path, "w");
            }
            if (fd < 0) {
                return false;
            }

            size_t len     = strlen(content);
            size_t written = kmod_fwrite(fd, content, len);
            kmod_fclose(fd);
            return written == len;
        }

        [[nodiscard]]
        bool sort_file_lines(const char *path) {
            if (path == nullptr) {
                return false;
            }

            int src = kmod_fopen(path, "r");
            if (src < 0) {
                return false;
            }

            char buffer[COPY_BUFFER_SIZE]{};
            char content[1024]{};
            size_t total = 0;
            while (total < sizeof(content) - 1) {
                size_t got = kmod_fread(src, buffer, sizeof(buffer));
                if (got == 0) {
                    break;
                }
                size_t copy_len = got;
                if (copy_len > sizeof(content) - 1 - total) {
                    copy_len = sizeof(content) - 1 - total;
                }
                memcpy(content + total, buffer, copy_len);
                total += copy_len;
                if (copy_len != got) {
                    kmod_fclose(src);
                    return false;
                }
            }
            kmod_fclose(src);

            const size_t MAX_LINES = 32;
            char *lines[MAX_LINES]{};
            size_t line_count = 0;
            char *cur         = content;
            while (*cur != '\0' && line_count < MAX_LINES) {
                lines[line_count++] = cur;
                char *newline       = strchr(cur, '\n');
                if (newline == nullptr) {
                    break;
                }
                *newline = '\0';
                cur      = newline + 1;
            }

            for (size_t i = 0; i < line_count; ++i) {
                for (size_t j = i + 1; j < line_count; ++j) {
                    if (strcmp(lines[i], lines[j]) > 0) {
                        auto *tmp = lines[i];
                        lines[i]  = lines[j];
                        lines[j]  = tmp;
                    }
                }
            }

            int dst = kmod_fopen(path, "w");
            if (dst < 0) {
                return false;
            }

            for (size_t i = 0; i < line_count; ++i) {
                size_t len = strlen(lines[i]);
                if (kmod_fwrite(dst, lines[i], len) != len ||
                    kmod_fwrite(dst, "\n", 1) != 1)
                {
                    kmod_fclose(dst);
                    return false;
                }
            }

            kmod_fclose(dst);
            return true;
        }

        [[nodiscard]]
        int run_simple_case(const RunnerContext &ctx, const OpenDirHandle &cwd,
                            const char *busybox_path,
                            const char *const argv[]) {
            const size_t MAX_ARGC = 8;
            const char *argv_buffer[MAX_ARGC]{};
            for (size_t i = 0; i < MAX_ARGC; ++i) {
                argv_buffer[i] = argv[i];
                if (argv[i] == nullptr) {
                    break;
                }
            }

            int status = 0;
            auto err = run_program(ctx, cwd, busybox_path, argv_buffer, status);
            if (err != RunProgramError::NONE) {
                return -1;
            }
            return run_exit_code(status);
        }

        [[nodiscard]]
        int run_background_sleep_kill_case(const RunnerContext &ctx,
                                           const OpenDirHandle &cwd,
                                           const char *busybox_path) {
            const char *sleep_argv[] = {"busybox", "sleep", "5", nullptr};
            CapIdx child_pcb         = cap::null;
            auto spawn_err =
                spawn_program(ctx, cwd, busybox_path, sleep_argv, child_pcb);
            if (spawn_err != RunProgramError::NONE) {
                return -1;
            }

            auto pid_res = sys_getpid(child_pcb).to_result();
            if (!pid_res.has_value()) {
                int status = 0;
                (void)wait_program(child_pcb, status);
                return -1;
            }

            char pid_buffer[32]{};
            int pid_len = snprintf(pid_buffer, sizeof(pid_buffer), "%lu",
                                   pid_res.value());
            if (pid_len <= 0 ||
                static_cast<size_t>(pid_len) >= sizeof(pid_buffer))
            {
                int status = 0;
                (void)wait_program(child_pcb, status);
                return -1;
            }

            const char *kill_argv[] = {"busybox", "kill", pid_buffer, nullptr};
            int kill_status         = 0;
            auto kill_err =
                run_program(ctx, cwd, busybox_path, kill_argv, kill_status);
            int sleep_status = 0;
            auto wait_err    = wait_program(child_pcb, sleep_status);
            if (kill_err != RunProgramError::NONE ||
                !run_status_success(kill_status) ||
                wait_err != RunProgramError::NONE)
            {
                return -1;
            }

            return 0;
        }

        [[nodiscard]]
        int run_sort_uniq_case(const RunnerContext &ctx,
                               const OpenDirHandle &cwd,
                               const char *busybox_path, const char *path) {
            if (!sort_file_lines(path)) {
                return -1;
            }
            const char *argv[] = {"busybox", "uniq", path, nullptr};
            return run_simple_case(ctx, cwd, busybox_path, argv);
        }

        [[nodiscard]]
        int run_busybox_case(const RunnerContext &ctx, const OpenDirHandle &cwd,
                             const char *busybox_path,
                             const BusyboxCase &testcase) {
            switch (testcase.kind) {
                case BusyboxCaseKind::SIMPLE:
                    return run_simple_case(ctx, cwd, busybox_path,
                                           testcase.argv);
                case BusyboxCaseKind::WRITE_FILE:
                    return write_file_content(testcase.file_path,
                                              testcase.file_content, false)
                               ? 0
                               : -1;
                case BusyboxCaseKind::APPEND_FILE:
                    return write_file_content(testcase.file_path,
                                              testcase.file_content, true)
                               ? 0
                               : -1;
                case BusyboxCaseKind::SORT_UNIQ_FILE:
                    return run_sort_uniq_case(ctx, cwd, busybox_path,
                                              testcase.file_path);
                case BusyboxCaseKind::CHECK_FILE_EXISTS: {
                    NodeMeta meta{};
                    if (!sys_vfs_stat(cwd.cap, testcase.file_path, &meta)) {
                        return 1;
                    }
                    return meta.type == EntryType::FILE ? 0 : 1;
                }
                case BusyboxCaseKind::BACKGROUND_SLEEP_KILL:
                    return run_background_sleep_kill_case(ctx, cwd,
                                                          busybox_path);
                default: return -1;
            }
        }
    }  // namespace

    TestRunStats run_busybox(const RunnerContext &ctx) {
        TestRunStats stats{};
        printf("#### OS COMP TEST GROUP START busybox-%s ####\n",
               ctx.libc_name);

        OpenDirHandle cwd{};
        if (!open_cwd_dir(ctx.libc_root, cwd)) {
            printf("#### OS COMP TEST GROUP END busybox-%s ####\n",
                   ctx.libc_name);
            return stats;
        }

        char busybox_path[PATH_BUFFER_SIZE]{};
        if (!make_path(busybox_path, sizeof(busybox_path), ctx.libc_root,
                       "busybox"))
        {
            printf("contest-runner: invalid busybox root %s\n", ctx.libc_root);
            close_cwd_dir(cwd);
            printf("#### OS COMP TEST GROUP END busybox-%s ####\n",
                   ctx.libc_name);
            return stats;
        }

        for (const auto &testcase : BUSYBOX_CASES) {
            ++stats.total;
            printf("contest-runner: busybox run %s\n", testcase.display);

            int ret = run_busybox_case(ctx, cwd, busybox_path, testcase);
            if (ret != 0 && strcmp(testcase.display, "false") != 0) {
                ++stats.failed;
                printf("testcase busybox %s fail\n", testcase.display);
                continue;
            }

            ++stats.passed;
            printf("testcase busybox %s success\n", testcase.display);
        }

        close_cwd_dir(cwd);
        printf("#### OS COMP TEST GROUP END busybox-%s ####\n", ctx.libc_name);
        return stats;
    }
}  // namespace contest_runner
