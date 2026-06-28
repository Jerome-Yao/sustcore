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

#include <sustcore/bootstrap.h>

#include "busybox.h"
#include "runner.h"

namespace contest_runner {
    namespace {
        constexpr size_t PATH_BUFFER_SIZE = 256;
        constexpr size_t MAX_ARGC         = 16;
        constexpr size_t MAX_ARG_BYTES    = 512;
        constexpr uint64_t PERM_STDIO_FILE =
            0x0002 | 0x010000 | 0x020000;

        struct BusyboxRedirect {
            bool present = false;
            bool append = false;
            char path[PATH_BUFFER_SIZE]{};
        };

        struct StdoutCapBootstrap {
            bsheader header;
            BootstrapCapExplainPayloadHead explain;
            char desc[12];
        };

        struct ShellIoBootstrap {
            bsheader header;
            BootstrapShellIoPayload payload;
        };

        struct PathBootstrap {
            bsheader header;
            char desc[PATH_BUFFER_SIZE + 16];
        };

        [[nodiscard]]
        bool resolve_redirect_path(const OpenDirHandle &cwd,
                                   const BusyboxRedirect &redirect,
                                   char resolved_path[PATH_BUFFER_SIZE]) {
            if (!redirect.present || resolved_path == nullptr) {
                return false;
            }
            if (redirect.path[0] == '/') {
                size_t len = strlen(redirect.path);
                if (len == 0 || len >= PATH_BUFFER_SIZE) {
                    return false;
                }
                memcpy(resolved_path, redirect.path, len + 1);
                return true;
            }
            if (cwd.path == nullptr || cwd.path[0] != '/') {
                return false;
            }
            int written = 0;
            if (strcmp(cwd.path, "/") == 0) {
                written = snprintf(resolved_path, PATH_BUFFER_SIZE, "/%s",
                                   redirect.path);
            } else {
                written = snprintf(resolved_path, PATH_BUFFER_SIZE, "%s/%s",
                                   cwd.path, redirect.path);
            }
            return written > 0 &&
                   static_cast<size_t>(written) < PATH_BUFFER_SIZE;
        }

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
        bool prepare_cloned_stdio_cap(CapIdx source_cap, CapIdx &prepared_cap) {
            prepared_cap = cap::null;
            if (source_cap == cap::null || source_cap == cap::error) {
                return false;
            }

            auto cap_res = sys_cap_clone(source_cap).to_result();
            if (!cap_res.has_value()) {
                return false;
            }

            prepared_cap = cap_res.value();
            auto downgrade_res =
                sys_cap_downgrade(prepared_cap, PERM_STDIO_FILE).to_result();
            if (!downgrade_res.has_value()) {
                (void)sys_cap_remove(prepared_cap).to_result();
                prepared_cap = cap::null;
                return false;
            }
            return true;
        }

        [[nodiscard]]
        bool is_space(char ch) noexcept {
            return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
        }

        [[nodiscard]]
        char decode_escape(char ch) noexcept {
            switch (ch) {
                case 'n':  return '\n';
                case '"':  return '"';
                case '\\': return '\\';
                default:   return ch;
            }
        }

        [[nodiscard]]
        bool parse_busybox_command_line(const char *line,
                                        const char *argv_out[MAX_ARGC],
                                        char storage[MAX_ARG_BYTES]) {
            if (line == nullptr || argv_out == nullptr || storage == nullptr) {
                return false;
            }

            for (size_t i = 0; i < MAX_ARGC; ++i) {
                argv_out[i] = nullptr;
            }

            size_t argc       = 0;
            size_t storage_at = 0;
            argv_out[argc++]  = "busybox";

            const char *cur = line;
            while (*cur != '\0') {
                while (is_space(*cur)) {
                    ++cur;
                }
                if (*cur == '\0') {
                    break;
                }
                if (argc + 1 >= MAX_ARGC) {
                    return false;
                }
                if (storage_at >= MAX_ARG_BYTES) {
                    return false;
                }

                argv_out[argc++] = &storage[storage_at];
                bool in_quotes    = false;
                while (*cur != '\0') {
                    if (!in_quotes && is_space(*cur)) {
                        break;
                    }
                    if (*cur == '"') {
                        in_quotes = !in_quotes;
                        ++cur;
                        continue;
                    }
                    if (*cur == '\\' && cur[1] != '\0') {
                        if (storage_at + 1 >= MAX_ARG_BYTES) {
                            return false;
                        }
                        storage[storage_at++] = decode_escape(cur[1]);
                        cur += 2;
                        continue;
                    }
                    if (storage_at + 1 >= MAX_ARG_BYTES) {
                        return false;
                    }
                    storage[storage_at++] = *cur++;
                }
                if (in_quotes) {
                    return false;
                }
                if (storage_at >= MAX_ARG_BYTES) {
                    return false;
                }
                storage[storage_at++] = '\0';
            }

            argv_out[argc] = nullptr;
            return argc > 1;
        }

        [[nodiscard]]
        bool extract_stdout_redirect(const char *argv_in[MAX_ARGC],
                                     const char *argv_out[MAX_ARGC],
                                     BusyboxRedirect &redirect) {
            if (argv_in == nullptr || argv_out == nullptr) {
                return false;
            }
            for (size_t i = 0; i < MAX_ARGC; ++i) {
                argv_out[i] = nullptr;
            }

            size_t out_idx = 0;
            for (size_t i = 0; argv_in[i] != nullptr; ++i) {
                if ((strcmp(argv_in[i], ">") == 0 ||
                     strcmp(argv_in[i], ">>") == 0))
                {
                    if (redirect.present || argv_in[i + 1] == nullptr) {
                        return false;
                    }
                    redirect.present = true;
                    redirect.append  = strcmp(argv_in[i], ">>") == 0;
                    size_t path_len  = strlen(argv_in[i + 1]);
                    if (path_len == 0 || path_len >= sizeof(redirect.path)) {
                        return false;
                    }
                    memcpy(redirect.path, argv_in[i + 1], path_len + 1);
                    ++i;
                    continue;
                }
                if (out_idx + 1 >= MAX_ARGC) {
                    return false;
                }
                argv_out[out_idx++] = argv_in[i];
            }
            argv_out[out_idx] = nullptr;
            return out_idx > 0;
        }

        [[nodiscard]]
        bool prepare_stdout_extra(const OpenDirHandle &cwd,
                                  const BusyboxRedirect &redirect,
                                  CapIdx extra_caps[5],
                                  StdoutCapBootstrap &stdout_cap_bootstrap,
                                  ShellIoBootstrap &shellio_bootstrap,
                                  PathBootstrap &stdout_path_bootstrap,
                                  const char *extra_bsargv[8]) {
            if (!redirect.present) {
                return false;
            }

            char resolved_path[PATH_BUFFER_SIZE]{};
            if (!resolve_redirect_path(cwd, redirect, resolved_path)) {
                return false;
            }

            int fd = redirect.append ? kmod_fopen(resolved_path, "a+") : -1;
            if (fd < 0 && !redirect.append) {
                (void)kmod_truncate(resolved_path, 0);
                fd = kmod_fopen(resolved_path, "w+");
            }
            if (fd < 0) {
                fd = kmod_mkfile(resolved_path, "w+");
            }
            if (fd >= 0) {
                kmod_fclose(fd);
            }

            fd = redirect.append ? kmod_fopen(resolved_path, "a+")
                                 : kmod_fopen(resolved_path, "w+");
            if (fd < 0) {
                return false;
            }

            CapIdx raw_cap = kmod_getcap(fd);
            if (raw_cap == cap::null || raw_cap == cap::error) {
                kmod_fclose(fd);
                return false;
            }
            CapIdx stdout_cap = cap::null;
            if (!prepare_cloned_stdio_cap(raw_cap, stdout_cap)) {
                kmod_fclose(fd);
                return false;
            }
            kmod_fclose(fd);

            memset(extra_caps, 0, sizeof(CapIdx) * 5);
            extra_caps[0] = stdout_cap;
            extra_caps[1] = cap::null;

            memset(&stdout_cap_bootstrap, 0, sizeof(stdout_cap_bootstrap));
            stdout_cap_bootstrap.header.size =
                sizeof(bsheader) + sizeof(BootstrapCapExplainPayloadHead) +
                strlen("#stdout-cap") + 1;
            stdout_cap_bootstrap.header.type      = boot::TYPE_CAPEXP;
            stdout_cap_bootstrap.explain.cap_idx  = stdout_cap;
            stdout_cap_bootstrap.explain.cap_type = PayloadType::VFILE;
            stdout_cap_bootstrap.explain.cap_perm = PERM_STDIO_FILE;
            strcpy(stdout_cap_bootstrap.desc, "#stdout-cap");

            shellio_bootstrap.header.size = sizeof(ShellIoBootstrap);
            shellio_bootstrap.header.type = boot::TYPE_SHELLIO;
            shellio_bootstrap.payload.flags =
                redirect.append ? boot::SHELLIO_FLAG_APPEND
                                : boot::SHELLIO_FLAG_OVERWRITE;
            shellio_bootstrap.payload.target = boot::SHELLIO_TARGET_STDOUT;

            memset(&stdout_path_bootstrap, 0, sizeof(stdout_path_bootstrap));
            int written =
                snprintf(stdout_path_bootstrap.desc,
                         sizeof(stdout_path_bootstrap.desc), "#stdout:%s",
                         resolved_path);
            if (written <= 0 || static_cast<size_t>(written) >=
                                    sizeof(stdout_path_bootstrap.desc))
            {
                (void)sys_cap_remove(stdout_cap).to_result();
                return false;
            }
            stdout_path_bootstrap.header.size =
                sizeof(bsheader) + static_cast<size_t>(written) + 1;
            stdout_path_bootstrap.header.type = boot::TYPE_PATHEXP;

            extra_bsargv[0] = reinterpret_cast<const char *>(&stdout_cap_bootstrap);
            extra_bsargv[1] = reinterpret_cast<const char *>(&shellio_bootstrap);
            extra_bsargv[2] = reinterpret_cast<const char *>(&stdout_path_bootstrap);
            extra_bsargv[3] = nullptr;
            return true;
        }

        [[nodiscard]]
        int run_busybox_command_line(const RunnerContext &ctx,
                                     const OpenDirHandle &cwd,
                                     const char *busybox_path,
                                     const char *line, bool dryrun) {
            const char *parsed_argv[MAX_ARGC]{};
            const char *exec_argv[MAX_ARGC]{};
            char storage[MAX_ARG_BYTES]{};
            if (!parse_busybox_command_line(line, parsed_argv, storage)) {
                printf("contest-runner: parse busybox command line failed %s\n",
                       line);
                return -1;
            }

            BusyboxRedirect redirect{};
            if (!extract_stdout_redirect(parsed_argv, exec_argv, redirect)) {
                printf("contest-runner: extract stdout redirect failed %s\n", line);
                return -1;
            }

            if (dryrun) {
                printf("program=%s, args=", busybox_path);
                bool first = true;
                for (size_t i = 1; exec_argv[i] != nullptr; ++i) {
                    if (!first) {
                        printf(" ");
                    }
                    printf("%s", exec_argv[i]);
                    first = false;
                }
                if (redirect.present) {
                    char resolved_path[PATH_BUFFER_SIZE]{};
                    if (!resolve_redirect_path(cwd, redirect, resolved_path)) {
                        return -1;
                    }
                    printf(", stdout=%s (%s)\n", resolved_path,
                           redirect.append ? "append" : "overwrite");
                } else {
                    printf(", stdout=(default)\n");
                }
                return 0;
            }

            ShellSpawnExtra extra{};
            CapIdx extra_caps[5]{};
            StdoutCapBootstrap stdout_cap_bootstrap{};
            ShellIoBootstrap shellio_bootstrap{};
            PathBootstrap stdout_path_bootstrap{};
            const char *extra_bsargv[8]{};
            if (redirect.present) {
                if (!prepare_stdout_extra(cwd, redirect, extra_caps,
                                          stdout_cap_bootstrap,
                                          shellio_bootstrap,
                                          stdout_path_bootstrap, extra_bsargv))
                {
                    printf("contest-runner: prepare stdout extra failed %s\n", line);
                    return -1;
                }
                extra.bsargv = extra_bsargv;
                extra.caps   = extra_caps;
            }

            int status = 0;
            auto err   = run_program(ctx, cwd, busybox_path, exec_argv, status,
                                     redirect.present ? &extra : nullptr);
            if (redirect.present && extra_caps[0] != cap::null &&
                extra_caps[0] != cap::error)
            {
                (void)sys_cap_remove(extra_caps[0]).to_result();
            }
            if (err != RunProgramError::NONE) {
                return -1;
            }
            return run_exit_code(status);
        }
    }  // namespace

    TestRunStats run_busybox(const RunnerContext &ctx, bool dryrun) {
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

        for (size_t i = 0; BUSYBOX_COMMAND_LINES[i] != nullptr; ++i) {
            const char *line = BUSYBOX_COMMAND_LINES[i];
            ++stats.total;
            printf("contest-runner: busybox run %s\n", line);

            int ret = run_busybox_command_line(ctx, cwd, busybox_path, line,
                                               dryrun);
            if (ret != 0 && strcmp(line, "false") != 0) {
                ++stats.failed;
                printf("testcase busybox %s fail\n", line);
                continue;
            }

            ++stats.passed;
            printf("testcase busybox %s success\n", line);
        }

        close_cwd_dir(cwd);
        printf("#### OS COMP TEST GROUP END busybox-%s ####\n", ctx.libc_name);
        return stats;
    }
}  // namespace contest_runner
