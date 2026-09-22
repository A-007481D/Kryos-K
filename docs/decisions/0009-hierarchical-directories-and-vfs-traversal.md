# 9. Hierarchical Directories, Path Traversal, and Directory Manipulation

Date: 2026-09-22

## Status

Accepted

## Context

Prior to Phase 23, Kryos VFS and KFS operations were essentially flat: paths were resolved as single-component lookups against the root directory (`/init.elf`, `/hello.elf`), without support for nested subdirectories, multi-component paths (`/a/b/c`), `.` and `..` relative traversal, or directory management syscalls (`mkdir`, `unlink`, `rmdir`).

In Phase 23, the kernel required full hierarchical filesystem traversal, directory creation, file and directory removal, and complete lifecycle integration across VFS, KFS, and syscall boundaries.

## Decision

1. **Hierarchical VFS Traversal (`vfs_resolve_path` and `vfs_resolve_parent`)**:
   - `vfs_resolve_path` breaks arbitrary path strings into individual components, skipping empty tokens and `.` references, and iteratively invoking `lookup()` across directory vnodes.
   - Enforces directory type checks on intermediate path components (returning `-ENOTDIR` when traversing a non-directory).
   - Enforces `-ENOTDIR` if a path specifies a trailing slash on a regular non-directory file (`/dir/file.txt/`).
   - `vfs_resolve_parent` reliably splits complex paths into the parent directory vnode and the final leaf name component, stripping trailing slashes and checking against `.` and `..` modifications.

2. **Subdirectory Initialization (`kfs_vnode_create`)**:
   - When creating a directory (`VNODE_TYPE_DIR`), KFS allocates a new directory inode and an initial data block.
   - Initial data block is formatted with standard `.` (12 bytes, pointing to the new directory's inode) and `..` (12 bytes, pointing to the parent directory's inode).
   - Directory inode size is initialized to 24 bytes, with `direct[0]` assigned to the block and `link_count = 2`.
   - The directory entry record is appended to the parent directory with proper 4-byte record alignment.

3. **Relative Traversal (`.` and `..`)**:
   - `kfs_vnode_lookup` synthesizes `.` to refer to the current vnode, and handles `..` on the root inode to refer to the root directory itself.
   - Subdirectories store `..` on disk, allowing standard directory traversal up the tree.

4. **File Unlinking and Directory Removal (`kfs_vnode_unlink` and `kfs_vnode_rmdir`)**:
   - `unlink` rejects directories (`-EISDIR`), frees all direct and indirect data blocks belonging to the target file inode, frees the inode in the bitmap and table, and compacts remaining entries in the parent directory.
   - `rmdir` verifies the target is a directory (`-ENOTDIR`), checks that the target directory contains no entries other than `.` and `..` (`-ENOTEMPTY`), frees its allocated block(s) and inode, and compacts the parent directory.
   - Parent directory compaction shifts trailing dirents left by `reclen`, updates `inode.size`, and frees any trailing disk blocks that become unreferenced, preventing block leaks and satisfying `kfsck.py` invariants.

5. **Syscall Integration (Syscalls 11, 12, 13)**:
   - Wired `SYS_MKDIR` (11), `SYS_UNLINK` (12), and `SYS_RMDIR` (13) into `syscall_dispatch`.
   - Validated userspace memory ranges with `user_range_readable()` and bounded path copying.
   - Added user library wrappers `mkdir()`, `unlink()`, and `rmdir()` in `user/libkryos/io.c`.
   - Added user CLI utilities `user/bin/mkdir.c` and `user/bin/rm.c`. Updated `user/bin/ls.c` to accept an optional directory path argument.

## Consequences

- Full multi-level directory trees can be created, populated, inspected, and cleanly deleted at runtime from both kernel space and userspace.
- All on-disk block and inode allocations remain strictly consistent, validated post-run by `scripts/kfsck.py` with zero leaked or unreferenced blocks.
- The system conforms to standard POSIX return codes for directory error conditions (`-EEXIST`, `-ENOENT`, `-ENOTEMPTY`, `-EISDIR`, `-ENOTDIR`, `-EINVAL`).
