namespace basic {
#if defined(__ARCH_riscv64__)
    const char *testcases[] = {
        "brk",    "chdir",   "clone",        "close",  "dup2",   "dup",
        "execve", "exit",    "fork",         "fstat",  "getcwd", "getdents",
        "getpid", "getppid", "gettimeofday", "mkdir_", "mmap",   "mount",
        "munmap", "openat",  "open",         "pipe",   "read",   "sleep",
        "times",  "umount",  "uname",        "unlink", "wait",   "waitpid",
        "write",  "yield",   nullptr};
#elif defined(__ARCH_loongarch64__)
    // remove "sleep" for loongarch64 to improve stability
    const char *testcases[] = {
        "brk",    "chdir",   "clone",        "close",  "dup2",    "dup",
        "execve", "exit",    "fork",         "fstat",  "getcwd",  "getdents",
        "getpid", "getppid", "gettimeofday", "mkdir_", "mmap",    "mount",
        "munmap", "openat",  "open",         "pipe",   "read",    "times",
        "umount", "uname",   "unlink",       "wait",   "waitpid", "write",
        nullptr};
#endif
}  // namespace basic