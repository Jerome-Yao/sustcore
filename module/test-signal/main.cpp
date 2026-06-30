/**
 * @file main.cpp
 * @author theflysong
 * @brief Linux ABI signal 测试
 * @version alpha-1.0.0
 * @date 2026-06-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cstddef>
#include <cstdint>

extern "C" long linux_write(size_t fd, const void *buf, size_t len);
extern "C" long linux_syscall(size_t a0, size_t a1, size_t a2, size_t a3,
                              size_t a4, size_t a5, size_t a6);

namespace {
    constexpr size_t STDOUT_FD           = 1;
    constexpr size_t __NR_read           = 63;
    constexpr size_t __NR_write          = 64;
    constexpr size_t __NR_close          = 57;
    constexpr size_t __NR_pipe2          = 59;
    constexpr size_t __NR_clone          = 220;
    constexpr size_t __NR_exit           = 93;
    constexpr size_t __NR_wait4          = 260;
    constexpr size_t __NR_getpid         = 172;
    constexpr size_t __NR_kill           = 129;
    constexpr size_t __NR_rt_sigaction   = 134;
    constexpr int LINUX_SIGUSR1          = 10;
    constexpr int TEST_SIGNAL_COUNT      = 1;
    constexpr uint64_t LINUX_SA_RESTORER = 0x04000000UL;

    struct linux_sigset_t {
        uint64_t sig[1];
    };

    struct linux_sigaction {
        size_t handler;
        uint64_t flags;
        size_t restorer;
        linux_sigset_t mask;
    };

    volatile size_t g_signal_counter = 0;
    volatile bool g_done             = false;

    size_t strlen(const char *s) {
        size_t len = 0;
        while (s != nullptr && s[len] != '\0') {
            ++len;
        }
        return len;
    }

    void puts(const char *s) {
        linux_write(STDOUT_FD, s, strlen(s));
    }

    void test_fail(const char *msg) {
        puts("test-signal: FAIL: ");
        puts(msg);
        puts("\n");
    }

    void test_pass(const char *msg) {
        puts("test-signal: PASS: ");
        puts(msg);
        puts("\n");
    }

    long linux_clone(size_t flags) {
        return linux_syscall(flags, 0, 0, 0, 0, 0, __NR_clone);
    }

    long linux_read(int fd, void *buf, size_t len) {
        return linux_syscall(static_cast<size_t>(fd),
                             reinterpret_cast<size_t>(buf), len, 0, 0, 0,
                             __NR_read);
    }

    long linux_write_sys(int fd, const void *buf, size_t len) {
        return linux_syscall(static_cast<size_t>(fd),
                             reinterpret_cast<size_t>(buf), len, 0, 0, 0,
                             __NR_write);
    }

    long linux_close(int fd) {
        return linux_syscall(static_cast<size_t>(fd), 0, 0, 0, 0, 0,
                             __NR_close);
    }

    long linux_pipe2(int pipefd[2], int flags) {
        return linux_syscall(reinterpret_cast<size_t>(pipefd),
                             static_cast<size_t>(flags), 0, 0, 0, 0,
                             __NR_pipe2);
    }

    long linux_exit(int code) {
        return linux_syscall(static_cast<size_t>(code), 0, 0, 0, 0, 0,
                             __NR_exit);
    }

    long linux_wait4(int pid, int *status, int options, void *rusage) {
        return linux_syscall(static_cast<size_t>(pid),
                             reinterpret_cast<size_t>(status),
                             static_cast<size_t>(options),
                             reinterpret_cast<size_t>(rusage), 0, 0,
                             __NR_wait4);
    }

    long linux_getpid() {
        return linux_syscall(0, 0, 0, 0, 0, 0, __NR_getpid);
    }

    long linux_kill(int pid, int sig) {
        return linux_syscall(static_cast<size_t>(pid),
                             static_cast<size_t>(sig), 0, 0, 0, 0,
                             __NR_kill);
    }

    long linux_rt_sigaction(int signo, const linux_sigaction *act,
                            linux_sigaction *oldact, size_t sigsetsize) {
        return linux_syscall(static_cast<size_t>(signo),
                             reinterpret_cast<size_t>(act),
                             reinterpret_cast<size_t>(oldact), sigsetsize,
                             0, 0, __NR_rt_sigaction);
    }

    extern "C" [[noreturn]] void user_rt_sigreturn_trampoline();

    extern "C" void signal_handler(int signo) {
        if (signo != LINUX_SIGUSR1) {
            test_fail("unexpected signo");
            return;
        }
        ++g_signal_counter;
        if (g_signal_counter >= TEST_SIGNAL_COUNT) {
            g_done = true;
        }
    }
}  // namespace

extern "C" [[noreturn]] void test_signal_main(size_t argc, const char *argv[],
                                              const char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    puts("test-signal: starting test\n");

    int ready_pipe[2] = {-1, -1};
    if (linux_pipe2(ready_pipe, 0) != 0) {
        test_fail("pipe2 failed");
        linux_exit(6);
        while (true) {
        }
    }

    long pid = linux_clone(17);
    if (pid < 0) {
        test_fail("clone failed");
        linux_exit(1);
        while (true) {
        }
    }

    if (pid == 0) {
        (void)linux_close(ready_pipe[0]);
        linux_sigaction action{};
        action.handler    = reinterpret_cast<size_t>(&signal_handler);
        action.flags      = 0;
        action.restorer   = 0;
        action.mask.sig[0] = 0;
        if (linux_rt_sigaction(LINUX_SIGUSR1, &action, nullptr,
                               sizeof(action.mask)) != 0)
        {
            test_fail("rt_sigaction failed");
            linux_exit(2);
            while (true) {
            }
        }
        char ready = 'R';
        if (linux_write_sys(ready_pipe[1], &ready, 1) != 1) {
            test_fail("ready pipe write failed");
            linux_exit(7);
            while (true) {
            }
        }
        (void)linux_close(ready_pipe[1]);

        volatile size_t spin = 0;
        while (!g_done) {
            ++spin;
        }
        if (g_signal_counter == TEST_SIGNAL_COUNT) {
            test_pass("async handler delivery");
            linux_exit(0);
        } else {
            test_fail("handler count mismatch");
            linux_exit(3);
        }
        while (true) {
        }
    }

    (void)linux_close(ready_pipe[1]);
    char ready = 0;
    if (linux_read(ready_pipe[0], &ready, 1) != 1 || ready != 'R') {
        test_fail("ready pipe read failed");
        linux_exit(8);
        while (true) {
        }
    }
    (void)linux_close(ready_pipe[0]);

    for (size_t i = 0; i < TEST_SIGNAL_COUNT; ++i) {
        long kill_res = linux_kill(static_cast<int>(pid), LINUX_SIGUSR1);
        if (kill_res != 0) {
            test_fail("kill(SIGUSR1) failed");
            linux_exit(4);
            while (true) {
            }
        }
    }

    int status = 0;
    long waited = linux_wait4(static_cast<int>(pid), &status, 0, nullptr);
    if (waited == pid && status == 0) {
        test_pass("wait4 child exit");
    } else {
        test_fail("wait4 fallback child failed");
        linux_exit(5);
        while (true) {
        }
    }

    g_signal_counter = 0;
    g_done           = false;

    int ready_pipe2[2] = {-1, -1};
    if (linux_pipe2(ready_pipe2, 0) != 0) {
        test_fail("pipe2 second failed");
        linux_exit(9);
        while (true) {
        }
    }

    pid = linux_clone(17);
    if (pid < 0) {
        test_fail("second clone failed");
        linux_exit(10);
        while (true) {
        }
    }

    if (pid == 0) {
        (void)linux_close(ready_pipe2[0]);
        linux_sigaction action{};
        action.handler     = reinterpret_cast<size_t>(&signal_handler);
        action.flags       = LINUX_SA_RESTORER;
        action.restorer    = reinterpret_cast<size_t>(&user_rt_sigreturn_trampoline);
        action.mask.sig[0] = 0;
        if (linux_rt_sigaction(LINUX_SIGUSR1, &action, nullptr,
                               sizeof(action.mask)) != 0)
        {
            test_fail("explicit rt_sigaction failed");
            linux_exit(11);
            while (true) {
            }
        }
        char ready2 = 'R';
        if (linux_write_sys(ready_pipe2[1], &ready2, 1) != 1) {
            test_fail("explicit ready pipe write failed");
            linux_exit(12);
            while (true) {
            }
        }
        (void)linux_close(ready_pipe2[1]);

        volatile size_t spin2 = 0;
        while (!g_done) {
            ++spin2;
        }
        if (g_signal_counter == TEST_SIGNAL_COUNT) {
            test_pass("explicit sa_restorer delivery");
            linux_exit(0);
        } else {
            test_fail("explicit handler count mismatch");
            linux_exit(13);
        }
        while (true) {
        }
    }

    (void)linux_close(ready_pipe2[1]);
    ready = 0;
    if (linux_read(ready_pipe2[0], &ready, 1) != 1 || ready != 'R') {
        test_fail("explicit ready pipe read failed");
        linux_exit(14);
        while (true) {
        }
    }
    (void)linux_close(ready_pipe2[0]);

    for (size_t i = 0; i < TEST_SIGNAL_COUNT; ++i) {
        long kill_res = linux_kill(static_cast<int>(pid), LINUX_SIGUSR1);
        if (kill_res != 0) {
            test_fail("explicit kill(SIGUSR1) failed");
            linux_exit(15);
            while (true) {
            }
        }
    }

    status = 0;
    waited = linux_wait4(static_cast<int>(pid), &status, 0, nullptr);
    if (waited == pid && status == 0) {
        test_pass("explicit wait4 child exit");
        test_pass("signal integration");
        linux_exit(0);
    }

    test_fail("wait4 explicit child failed");
    linux_exit(16);
    while (true) {
    }
}
