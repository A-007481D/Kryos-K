#!/usr/bin/env python3
import sys
import struct
import math

KFS_MAGIC = 0x4B465331
KFS_BLOCK_SIZE = 512

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <disk.img>")
        sys.exit(1)
        
    img = sys.argv[1]
    
    with open(img, "rb") as f:
        data = f.read()
        
    if len(data) < KFS_BLOCK_SIZE:
        print("[FAIL] Image too small for superblock")
        sys.exit(1)
        
    sb = data[:512]
    magic, total_blocks, inode_count, \
    bmap_start, bmap_blks, \
    imap_start, imap_blks, \
    itable_start, itable_blks, \
    data_start, root_inode = struct.unpack("<11I", sb[:44])
    
    if magic != KFS_MAGIC:
        print(f"[FAIL] Invalid magic: {hex(magic)}")
        sys.exit(1)
        
    print(f"KFS Superblock valid.")
    print(f"  total_blocks: {total_blocks}, data_start: {data_start}")
    
    if len(data) != total_blocks * KFS_BLOCK_SIZE:
        print(f"[FAIL] Image size ({len(data)}) does not match superblock total_blocks ({total_blocks * KFS_BLOCK_SIZE})")
        sys.exit(1)
        
    def get_bit(start_blk, bit_idx):
        byte_idx = bit_idx // 8
        bit_off = bit_idx % 8
        offset = start_blk * KFS_BLOCK_SIZE + byte_idx
        return (data[offset] & (1 << bit_off)) != 0

    # Read inodes
    block_refs = set()
    errors = 0
    
    def mark_block(blk):
        nonlocal errors
        if blk < data_start or blk >= total_blocks:
            print(f"[FAIL] Block reference out of bounds: {blk}")
            errors += 1
            return
        if blk in block_refs:
            print(f"[FAIL] Duplicate block reference: {blk}")
            errors += 1
            return
        if not get_bit(bmap_start, blk):
            print(f"[FAIL] Block {blk} referenced but not marked in bitmap")
            errors += 1
            return
        block_refs.add(blk)

    for ino in range(inode_count):
        if get_bit(imap_start, ino):
            ioff = itable_start * KFS_BLOCK_SIZE + (ino * 64)
            inode_data = data[ioff:ioff+64]
            size, typ = struct.unpack("<2I", inode_data[:8])
            direct = struct.unpack("<10I", inode_data[8:48])
            indirect = struct.unpack("<I", inode_data[48:52])[0]
            
            if typ not in (1, 2):
                print(f"[FAIL] Inode {ino} has invalid type {typ}")
                errors += 1
                
            blocks_needed = math.ceil(size / KFS_BLOCK_SIZE)
            
            for i in range(min(10, blocks_needed)):
                mark_block(direct[i])
                
            if blocks_needed > 10:
                if indirect == 0:
                    print(f"[FAIL] Inode {ino} needs indirect block but pointer is 0")
                    errors += 1
                else:
                    mark_block(indirect)
                    ind_off = indirect * KFS_BLOCK_SIZE
                    ind_data = data[ind_off:ind_off+512]
                    ind_blocks = struct.unpack("<128I", ind_data)
                    for i in range(blocks_needed - 10):
                        mark_block(ind_blocks[i])
                        
            # If directory, validate dirents
            if typ == 2:
                # Read all data
                dir_data = bytearray()
                for i in range(min(10, blocks_needed)):
                    blk_off = direct[i] * KFS_BLOCK_SIZE
                    dir_data += data[blk_off:blk_off+KFS_BLOCK_SIZE]
                if blocks_needed > 10:
                    ind_off = indirect * KFS_BLOCK_SIZE
                    ind_blocks = struct.unpack("<128I", data[ind_off:ind_off+512])
                    for i in range(blocks_needed - 10):
                        blk_off = ind_blocks[i] * KFS_BLOCK_SIZE
                        dir_data += data[blk_off:blk_off+KFS_BLOCK_SIZE]
                        
                dir_data = dir_data[:size]
                offset = 0
                while offset < size:
                    d_ino, d_reclen, d_typ = struct.unpack_from("<IHB", dir_data, offset)
                    if d_reclen == 0 or d_reclen % 4 != 0:
                        print(f"[FAIL] Inode {ino} has dirent at {offset} with invalid reclen {d_reclen}")
                        errors += 1
                        break
                    
                    if d_ino >= inode_count:
                        print(f"[FAIL] Inode {ino} dirent has out-of-bounds inode {d_ino}")
                        errors += 1
                        
                    offset += d_reclen
                    
    # Validate metadata block reservations
    for blk in range(data_start):
        if not get_bit(bmap_start, blk):
            print(f"[FAIL] Metadata block {blk} is NOT reserved in block bitmap")
            errors += 1

    # Check unreferenced blocks
    for blk in range(data_start, total_blocks):
        if get_bit(bmap_start, blk) and blk not in block_refs:
            # It could be an unreferenced block, which is technically a leak but maybe valid if it's reserved
            # In our mkfs, we only mark exactly what we use.
            print(f"[FAIL] Block {blk} marked in bitmap but not referenced by any inode")
            errors += 1

    if errors > 0:
        print(f"KFS Check failed with {errors} errors.")
        sys.exit(1)
        
    print("KFS Check passed: all invariants hold.")
    sys.exit(0)

if __name__ == "__main__":
    main()
