# Ext4 Development Notes

## Known Pitfalls

### VFS::open auto-creates files only with O_CREAT
`VFS::open()` (kernel/vfs/vfs.cpp:654-655) only auto-calls `mkfile()` when both conditions are met:
1. `_open_file()` returns `ENTRY_NOT_FOUND`
2. `oflags & flags::O_CREAT` is set

Without `O_CREAT`, `fopen(path, "r")` on a non-existent file returns an error instead of silently creating it.

### BufferHandler only covers _dev_block_size
`BufferCache` handlers cover `_dev_block_size` (128 bytes for virtio-blk), NOT `_block_size` (1024 for ext4). Always use `read_fs_block`/`write_fs_block` for directory block access — they handle multi-device-block IO. Never use `get_buffer_async()` directly for ext4 block operations.

### Ext4Directory cache invalidation
`_entries_cached` is set to false in `mkfile`, `mkdir`, `unlink`, `rmdir`, `rename`. However `VSuperblock::get_vnode()` may return a different `Ext4Directory` instance for the same inode. This is a separate issue from VINode-level cache eviction (below) — it concerns directory entry cache coherence across VINode instances, not inode type mismatch.

### Buffer cache write visibility
Writes via `write_fs_block` go to `BufferCache` and mark buffers dirty. `read_fs_block` reads from the same cache. If a dirty buffer is evicted (unlikely with 8192 slots), data is lost. For critical operations (rename, unlink), call `sync()` after writes to flush dirty buffers to disk.

## VINode Cache Rules

### Rule: Always evict VINode cache when an inode is freed

`VSuperblock::_inode_cache` (kernel/vfs/vfs.cpp:43) caches `VINode*` by inode number. When an inode is freed and later reallocated (possibly as a different type — file→dir or vice versa), a stale cache hit returns the old VINode whose `IINode` type no longer matches the on-disk inode. This causes `as_directory()` / `as_file()` to return `INVALID_PARAM`.

**Must do**: After `Ext4Superblock::release_file_inode()` is called (via unlink/rmdir/delete), call `VSuperblock::evict_inode(inode_id)` on the parent's VSuperblock.

**Where**: `VFS::unlink()` and `VFS::rmdir()` (kernel/vfs/vfs.cpp). Look up the target inode_id via `lookup()` before the delete, then `evict_inode()` after.

**Why not in ext4 layer**: `Ext4Superblock` has no reference to `VSuperblock`. The VFS layer bridges them.

### Safety net: fallback eviction in `_open_dir()`

Even if unlink/rmdir eviction is missed, `_open_dir()` (line ~580) retries with cache eviction when `as_directory()` fails on a resolved VINode. This catches cases where the cached VINode's IINode type is stale.

### Pattern

```cpp
// In VFS::unlink / VFS::rmdir:
auto lookup_res = parent_dir->lookup(name);   // get inode_id before delete
propagate(lookup_res);
auto del_res = parent_dir->unlink(name);       // or rmdir
propagate(del_res);
parent_vinode->superblock().evict_inode(lookup_res.value());  // CRITICAL
```

## Adding a New Test Module

Test modules are user-space programs compiled into `.mod` files, loaded from initrd by the init module.

### Step 1: Create module directory
```
module/test_MYNAME/
├── main.cpp       # test code
├── include.mk     # sources += main.cpp
├── options.mk     # component-name, module-output, flags
└── Makefile       # copy from another test module
```

### Step 2: Register in Makefile
In the top-level `Makefile`:
1. Add to `module-components` list (line 36)
2. Add `module-component-makefile.test_MYNAME` entry (line ~62)
3. Add explicit build line in `build-mods` target (line ~98):
```
$(q)$(MAKE) -f $(module-component-makefile.test_MYNAME) $(arg-basic) build
```

### Step 3: Set component name in options.mk
```
component-name := test_MYNAME
module-output := test_MYNAME.mod
```

### Step 4: Add to init module
In `module/init/main.cpp`, add spawn code after existing tests:
```cpp
fd = kmod_fopen("/initrd/test_MYNAME.mod", "x");
if (fd >= 0) {
    if (spawn_with_root_dir(fd, SCHED_CLASS_RR, root_dir_cap) == cap::error)
        printf("init: create test_MYNAME failed\n");
    else
        printf("init: MYNAME test spawned\n");
    kmod_fclose(fd);
}
```

Tests run **sequentially** — each `spawn_with_root_dir` blocks until the child process exits.

### kmod API differences vs POSIX
- No `open(path, flags)` → use `kmod_mkfile(path, "w+")` or `kmod_fopen(path, "r"/"w")`
- `kmod_mkfile` creates or fails if file exists (returns error, no overwrite)
- No `lseek` → sequential I/O only via `kmod_fread`/`kmod_fwrite`
- No `stat` → use `sys_vfs_size(kmod_getcap(fd))` for file size
- Directory listing: `sys_vfs_opendir` + `sys_vfs_getdents`
- `exit(0)` for PASS, `exit(-1)` for FAIL
