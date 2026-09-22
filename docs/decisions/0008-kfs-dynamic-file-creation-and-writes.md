# 8. KFS Dynamic File Creation and Inode/Block Allocation

Date: 2026-09-22

## Status

Accepted

## Context

In Phase 21, the Kryos File System (KFS) was introduced as an on-disk filesystem, but remained read-only during kernel execution. File creation, block allocation, and file writing relied entirely on the offline tooling `scripts/mkfs_kfs.py`.

In Phase 22, the kernel and userspace required runtime writable filesystem capabilities: creating new files (`O_CREAT`), writing file content (`write`), file truncation (`O_TRUNC`), append mode (`O_APPEND`), and allocating/freeing inodes and data blocks dynamically on `/dev/hda`.

## Decision

1. **Bitmap Allocation & Deallocation**:
   - Inode allocation (`kfs_alloc_inode`): Scans `inode_bitmap` for free bits, writes the updated bitmap sector to disk, and initializes the inode table entry.
   - Block allocation (`kfs_alloc_block`): Scans `block_bitmap` starting from `KFS_DATA_BLOCKS_START` (block 90), writes the updated bitmap sector to disk, and zeroes out the allocated data block.
   - Block deallocation (`kfs_free_block`) and inode deallocation (`kfs_free_inode`): Clears bitmap bits and writes back to disk.

2. **File Writing (`kfs_vnode_write`)**:
   - Supports writing across direct blocks (blocks 0–9) and allocating/managing a singly-indirect block (blocks 10–137), accommodating files up to 70.6 KiB.
   - Handles partial sector reads when writing non-aligned chunks.
   - Updates `inode.size` and persists modified inode metadata.
   - Respects open flags: rejects writes when opened without `O_WRONLY` or `O_RDWR`.

3. **File Creation (`kfs_vnode_create`)**:
   - Enforces 4-byte aligned directory entry records (`struct kfs_dirent`) matching `kfsck.py` specifications.
   - Appends dirents to the parent directory via directory writes and updates parent directory size.

4. **VFS & Syscall Open Flags**:
   - Added standard POSIX flags (`O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_EXCL`, `O_TRUNC`, `O_APPEND`).
   - Wired `sys_open` to validate against supported flags and delegate creation/truncation to VFS.

5. **Userspace Utilities**:
   - Added `user/bin/cat.c` (`cat.elf`) to print file contents to standard output.

## Consequences

- Kryos supports full end-to-end persistent file creation and writing from both kernel and userspace.
- Post-execution verification via `scripts/kfsck.py` validates that all runtime block allocations and directory structures maintain on-disk invariants with zero leaks.
