/**
 * @file main.cpp
 * @brief Ext4 read-write and mkdir tests
 */

#include <kmod/bootstrap.h>
#include <kmod/syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    constexpr const char *RW_FILE     = "/test_img/rw_test_file";
    constexpr const char *RW_NAME     = "rw_test_file";
    constexpr const char *MKDIR_PATH  = "/test_img/rw_test_dir";
    constexpr const char *MKDIR_NAME  = "rw_test_dir";
    constexpr const char *TEST_STR    = "hello ext4 write!";
    constexpr size_t TEST_STR_LEN     = 18;
    char g_read_buf[256];
    char g_dirent_buf[16384];
    char g_entry_name_buf[256];

    [[nodiscard]]
    CapIdx bootstrap_root_dir() {
        CapIdx cap = cap::null;
        bool found = false;
        bool ok    = bootstrap_foreach_record(
            __startup_data, __startup_size,
            [&](const BootstrapRecordView &view) {
                if (found || view.header->type != BOOTSTRAP_TYPE_DIRCAPEXPLAIN)
                    return;
                BootstrapCapPathView cap_path {};
                if (!bootstrap_parse_cap_path(view, cap_path)) return;
                if (strcmp(cap_path.path, "/") != 0) return;
                cap   = cap_path.cap;
                found = true;
            });
        return ok && found ? cap : cap::null;
    }

    [[nodiscard]]
    bool dir_has_entry(CapIdx dir_cap, const char *entry_name, bool expect_file) {
        if (entry_name == nullptr) return false;
        const size_t entry_name_len = strlen(entry_name);
        if (entry_name_len >= sizeof(g_entry_name_buf)) return false;
        memcpy(g_entry_name_buf, entry_name, entry_name_len + 1);
        size_t doff = 0;
        while (doff != DIR_ENTRY_END) {
            CapInfo cap_info {};
            if (!sys_cap_lookup(dir_cap, &cap_info) ||
                cap_info.type != PayloadType::VDIR) return false;
            memset(g_dirent_buf, 0, sizeof(g_dirent_buf));
            const size_t bytes =
                sys_vfs_getdents(dir_cap, g_dirent_buf, sizeof(g_dirent_buf), doff);
            if (bytes == 0) return false;
            size_t parsed = 0;
            for (size_t offset = 0; offset < bytes;) {
                if (bytes - offset < sizeof(dir_entry_header)) return false;
                auto *header = reinterpret_cast<const dir_entry_header *>(
                    g_dirent_buf + offset);
                const char *name = g_dirent_buf + offset + sizeof(dir_entry_header);
                const size_t name_room = bytes - offset - sizeof(dir_entry_header);
                if (memchr(name, '\0', name_room) == nullptr) return false;
                const size_t name_len = strlen(name);
                if (name_len == entry_name_len &&
                    memcmp(name, g_entry_name_buf, entry_name_len) == 0 &&
                    header->is_file == expect_file)
                    return true;
                ++parsed;
                if (header->next_offset == DIR_ENTRY_END) return false;
                if (header->next_offset == 0 || offset + header->next_offset > bytes)
                    return false;
                offset += header->next_offset;
            }
            if (parsed == 0) return false;
            doff += parsed;
        }
        return false;
    }
}  // namespace

int kmod_main() {
    printf("test_ext4_rw: start pid=%u\n", sys_getpid(__pcb_cap));

    CapIdx root_cap = bootstrap_root_dir();
    if (root_cap == cap::null || root_cap == cap::error) {
        printf("test_ext4_rw: bootstrap root dir missing\n");
        exit(-1);
    }

    // --- write test ---
    printf("test_ext4_rw: creating %s\n", RW_FILE);
    int fd = kmod_mkfile(RW_FILE, "w+");
    if (fd < 0) {
        fd = kmod_fopen(RW_FILE, "r+");
        if (fd < 0) {
            printf("test_ext4_rw: create/open %s failed\n", RW_FILE);
            exit(-1);
        }
    }

    size_t written = kmod_fwrite(fd, TEST_STR, TEST_STR_LEN);
    if (written != TEST_STR_LEN) {
        printf("test_ext4_rw: write failed wrote=%u expected=%u\n",
               static_cast<unsigned>(written),
               static_cast<unsigned>(TEST_STR_LEN));
        kmod_fclose(fd);
        exit(-1);
    }
    printf("test_ext4_rw: wrote %u bytes\n", static_cast<unsigned>(written));

    const size_t file_size = sys_vfs_size(kmod_getcap(fd));
    if (file_size != TEST_STR_LEN) {
        printf("test_ext4_rw: size mismatch after write got=%u expected=%u\n",
               static_cast<unsigned>(file_size),
               static_cast<unsigned>(TEST_STR_LEN));
        kmod_fclose(fd);
        exit(-1);
    }

    kmod_fclose(fd);

    // reopen for reading (offset back to 0)
    fd = kmod_fopen(RW_FILE, "r");
    if (fd < 0) {
        printf("test_ext4_rw: reopen for read failed\n");
        exit(-1);
    }
    memset(g_read_buf, 0, sizeof(g_read_buf));
    size_t got = kmod_fread(fd, g_read_buf, TEST_STR_LEN);
    if (got != TEST_STR_LEN) {
        printf("test_ext4_rw: read failed got=%u expected=%u\n",
               static_cast<unsigned>(got),
               static_cast<unsigned>(TEST_STR_LEN));
        kmod_fclose(fd);
        exit(-1);
    }
    kmod_fclose(fd);

    if (memcmp(g_read_buf, TEST_STR, TEST_STR_LEN) != 0) {
        printf("test_ext4_rw: data mismatch '%.18s' vs '%.18s'\n",
               g_read_buf, TEST_STR);
        exit(-1);
    }
    printf("test_ext4_rw: write+read PASS\n");

    // --- large write test (exercises extent split) ---
    printf("test_ext4_rw: large write test\n");
    kmod_fclose(fd);
    fd = kmod_mkfile("/test_img/large_file", "w+");
    if (fd < 0) {
        fd = kmod_fopen("/test_img/large_file", "r+");
        if (fd < 0) {
            printf("test_ext4_rw: create large_file failed\n");
            exit(-1);
        }
    }
    constexpr size_t kLargeSize = 10240;
    char large_buf[kLargeSize];
    for (size_t i = 0; i < kLargeSize; ++i) large_buf[i] = static_cast<char>(i & 0xFF);
    written = kmod_fwrite(fd, large_buf, kLargeSize);
    if (written != kLargeSize) {
        printf("test_ext4_rw: large write failed wrote=%u\n",
               static_cast<unsigned>(written));
        kmod_fclose(fd);
        exit(-1);
    }
    kmod_fclose(fd);
    fd = kmod_fopen("/test_img/large_file", "r");
    if (fd < 0) { printf("test_ext4_rw: reopen large_file failed\n"); exit(-1); }
    memset(large_buf, 0, kLargeSize);
    got = kmod_fread(fd, large_buf, kLargeSize);
    kmod_fclose(fd);
    if (got != kLargeSize) {
        printf("test_ext4_rw: large read failed got=%u\n",
               static_cast<unsigned>(got));
        exit(-1);
    }
    for (size_t i = 0; i < kLargeSize; ++i) {
        if (static_cast<unsigned char>(large_buf[i]) != static_cast<unsigned char>(i & 0xFF)) {
            printf("test_ext4_rw: large data mismatch at %u\n",
                   static_cast<unsigned>(i));
            exit(-1);
        }
    }
    kmod_fclose(fd);
    printf("test_ext4_rw: large write+read PASS\n");

    // verify file visible in directory
    CapIdx ext4_dir = sys_vfs_opendir(root_cap, "test_img", flags::O_READ);
    if (ext4_dir == cap::null || ext4_dir == cap::error) {
        printf("test_ext4_rw: opendir test_img failed\n");
        exit(-1);
    }
    if (!dir_has_entry(ext4_dir, RW_NAME, true)) {
        printf("test_ext4_rw: created file not found in dir\n");
        sys_cap_remove(ext4_dir);
        exit(-1);
    }
    sys_cap_remove(ext4_dir);

    // --- mkdir test ---
    printf("test_ext4_rw: mkdir %s\n", MKDIR_PATH);
    int md = kmod_mkdir(MKDIR_PATH);
    if (md < 0) {
        printf("test_ext4_rw: mkdir failed\n");
        exit(-1);
    }

    ext4_dir = sys_vfs_opendir(root_cap, "test_img", flags::O_READ);
    if (ext4_dir == cap::null || ext4_dir == cap::error) {
        printf("test_ext4_rw: opendir for mkdir check failed\n");
        exit(-1);
    }
    if (!dir_has_entry(ext4_dir, MKDIR_NAME, false)) {
        printf("test_ext4_rw: created dir not found\n");
        sys_cap_remove(ext4_dir);
        exit(-1);
    }
    sys_cap_remove(ext4_dir);

    // verify subdir is openable (read_directory filters "." / "..")
    CapIdx sub_dir = sys_vfs_opendir(root_cap, "test_img/rw_test_dir", flags::O_READ);
    if (sub_dir == cap::null || sub_dir == cap::error) {
        printf("test_ext4_rw: opendir subdir failed\n");
        exit(-1);
    }
    sys_cap_remove(sub_dir);
    printf("test_ext4_rw: mkdir PASS\n");

    // --- unlink test ---
    printf("test_ext4_rw: unlink %s\n", RW_FILE);
    int ret = kmod_unlink(RW_FILE);
    if (ret < 0) {
        printf("test_ext4_rw: unlink failed\n");
        exit(-1);
    }

    // verify file no longer visible
    ext4_dir = sys_vfs_opendir(root_cap, "test_img", flags::O_READ);
    if (ext4_dir == cap::null || ext4_dir == cap::error) {
        printf("test_ext4_rw: opendir for unlink check failed\n");
        exit(-1);
    }
    if (dir_has_entry(ext4_dir, RW_NAME, true)) {
        printf("test_ext4_rw: unlinked file still visible!\n");
        sys_cap_remove(ext4_dir);
        exit(-1);
    }
    sys_cap_remove(ext4_dir);
    printf("test_ext4_rw: unlink PASS\n");

    // --- rmdir test ---
    printf("test_ext4_rw: rmdir %s\n", MKDIR_PATH);
    ret = kmod_rmdir(MKDIR_PATH);
    if (ret < 0) {
        printf("test_ext4_rw: rmdir failed\n");
        exit(-1);
    }
    ext4_dir = sys_vfs_opendir(root_cap, "test_img", flags::O_READ);
    if (ext4_dir == cap::null || ext4_dir == cap::error) {
        printf("test_ext4_rw: opendir for rmdir check failed\n");
        exit(-1);
    }
    if (dir_has_entry(ext4_dir, MKDIR_NAME, false)) {
        printf("test_ext4_rw: removed dir still visible!\n");
        sys_cap_remove(ext4_dir);
        exit(-1);
    }
    sys_cap_remove(ext4_dir);
    printf("test_ext4_rw: rmdir PASS\n");

    printf("test_ext4_rw: ALL PASS\n");
    exit(0);
    return 0;
}
