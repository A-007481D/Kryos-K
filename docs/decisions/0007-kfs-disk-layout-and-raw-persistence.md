# 7. KFS Disk Layout and Boot Sector Reservation

Date: 2026-09-22

## Status

Accepted

## Context

In Phase 20, raw ATA block read and write capabilities were implemented alongside persistence tests that verified disk writes across reboot cycles on `/dev/hda`.

In Phase 21, the Kryos File System (KFS) was implemented to replace the read-only in-memory TarFS ramdisk with a true persistent on-disk filesystem. Initially, the KFS superblock was placed at LBA 0. However, LBA 0 through 63 are traditionally reserved for MBR/bootloader sectors and raw persistence testing. When raw block tests wrote to early sectors, they overwrote the KFS superblock magic and corrupted filesystem metadata.

## Decision

1. **Boot and Raw Reservation**: The first 64 blocks (32 KiB) of `/dev/hda` (blocks 0–63) are strictly reserved for boot sectors, partition tables, and raw persistence tests.
2. **KFS Layout**:
   - Superblock: Fixed at block 64 (`KFS_SUPERBLOCK_BLK = 64`).
   - Inode Bitmap: 1 block at block 65 (`KFS_INODE_BITMAP_BLK = 65`).
   - Block Bitmap: 8 blocks starting at block 66 (`KFS_DATA_BITMAP_BLK = 66`).
   - Inode Table: 16 blocks starting at block 74 (`KFS_INODE_TABLE_START = 74`).
   - Data Blocks: Block 90 onwards (`KFS_DATA_BLOCKS_START = 90`).
3. **Format & Integrity Tooling**:
   - `scripts/mkfs_kfs.py` formats the raw image conforming to this layout and embeds initial user binaries.
   - `scripts/kfsck.py` performs offline static integrity checks (superblock magic, bitmap consistency, directory entry traversal, block pointer validity) after test suite execution.

## Consequences

- Raw block persistence tests and KFS coexist safely on `/dev/hda` without corrupting filesystem structures.
- Offline filesystem integrity can be asserted deterministically in CI via `kfsck.py`.
- Disk geometry and block sizes are standardized at 512 bytes per sector/block.
