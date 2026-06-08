/**
 * @file file.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kmod 文件描述符封装
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kmod/syscall.h>
#include <prm.h>

#include <cstdio>
#include <cstring>

namespace {
    struct FileStructure {
        CapIdx cap    = cap::null;
        size_t offset = 0;
        bool used     = false;
    };

    // allow up to 32 open files simultaneously,
    // which is temporarily sufficient for our needs
    constexpr int MAX_KMOD_FILES = 32;
    FileStructure g_files[MAX_KMOD_FILES]{};
    CapIdx g_initrd_cap = cap::null;

    // to find the free slot to insert the file
    [[nodiscard]]
    int alloc_fd() {
        for (int i = 0; i < MAX_KMOD_FILES; ++i) {
            if (!g_files[i].used) {
                g_files[i].used   = true;
                g_files[i].cap    = cap::null;
                g_files[i].offset = 0;
                return i;
            }
        }
        return -1;
    }

    // to ensure the capability refers to '/initrd/' is temporarily usable
    [[nodiscard]]
    bool ensure_initrd_cap() {
        if (g_initrd_cap != cap::null) {
            return true;
        }
        g_initrd_cap = sys_open_initrd();
        return g_initrd_cap != cap::error && g_initrd_cap != cap::null;
    }
    
    // to strip the prefix of 'initrd' to get the relative path for vfs open
    [[nodiscard]]
    const char *strip_initrd_prefix(const char *path) {
        constexpr const char *INITRD_PREFIX = "/initrd/";
        constexpr size_t INITRD_PREFIX_LEN  = 8;
        if (path == nullptr) {
            return nullptr;
        }
        if (strncmp(path, INITRD_PREFIX, INITRD_PREFIX_LEN) != 0) {
            return nullptr;
        }
        return path + INITRD_PREFIX_LEN;
    }

    [[nodiscard]]
    bool parse_open_options(const char *options, flags::oflg_t &oflags,
                            bool &append) {
        oflags = 0;
        append = false;
        if (options == nullptr) {
            return false;
        }
        if (strcmp(options, "r") == 0) {
            oflags = flags::O_READ;
            return true;
        }
        if (strcmp(options, "r+") == 0) {
            oflags = flags::O_READ | flags::O_WRITE;
            return true;
        }
        if (strcmp(options, "w") == 0) {
            oflags = flags::O_WRITE;
            return true;
        }
        if (strcmp(options, "w+") == 0) {
            oflags = flags::O_READ | flags::O_WRITE;
            return true;
        }
        if (strcmp(options, "a") == 0) {
            oflags = flags::O_WRITE;
            append = true;
            return true;
        }
        if (strcmp(options, "a+") == 0) {
            oflags = flags::O_READ | flags::O_WRITE;
            append = true;
            return true;
        }
        if (strcmp(options, "x") == 0) {
            oflags = flags::O_EXECUTE;
            return true;
        }
        return false;
    }

    // fd -> FileStructure
    [[nodiscard]]
    FileStructure *lookup_fd(int fd) {
        if (fd < 0 || fd >= MAX_KMOD_FILES || !g_files[fd].used) {
            return nullptr;
        }
        return &g_files[fd];
    }
}  // namespace

extern "C" {
int kmod_fopen(const char *path, const char *options) {
    flags::oflg_t oflags = 0;
    bool append          = false;
    const char *relpath  = strip_initrd_prefix(path);
    if (!parse_open_options(options, oflags, append) ||
        !ensure_initrd_cap() || relpath == nullptr || *relpath == '\0')
    {
        return -1;
    }

    int fd = alloc_fd();
    if (fd < 0) {
        return -1;
    }

    CapIdx cap = sys_vfs_open(g_initrd_cap, relpath, oflags);
    if (cap == cap::error || cap == cap::null) {
        g_files[fd] = {};
        return -1;
    }

    g_files[fd].cap = cap;
    if (append) {
        g_files[fd].offset = sys_vfs_size(cap);
    }
    return fd;
}

size_t kmod_fread(int fd, void *buf, size_t len) {
    auto *file = lookup_fd(fd);
    if (file == nullptr || buf == nullptr) {
        return 0;
    }
    size_t got    = sys_vfs_read(file->cap, file->offset, buf, len);
    file->offset += got;
    return got;
}

size_t kmod_fwrite(int fd, const void *buf, size_t len) {
    auto *file = lookup_fd(fd);
    if (file == nullptr || buf == nullptr) {
        return 0;
    }
    size_t written  = sys_vfs_write(file->cap, file->offset, buf, len);
    file->offset   += written;
    return written;
}

CapIdx kmod_getcap(int fd) {
    auto *file = lookup_fd(fd);
    if (file == nullptr) {
        return cap::error;
    }
    return file->cap;
}

void kmod_fclose(int fd) {
    auto *file = lookup_fd(fd);
    if (file == nullptr) {
        return;
    }
    if (file->cap != cap::null && file->cap != cap::error) {
        sys_cap_remove(file->cap);
    }
    *file = {};
}
}
