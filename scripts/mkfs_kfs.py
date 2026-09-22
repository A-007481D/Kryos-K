#!/usr/bin/env python3
import sys
import struct
import math
import os

KFS_MAGIC = 0x4B465331
KFS_BLOCK_SIZE = 512
KFS_TOTAL_BLOCKS = 32768
KFS_INODE_COUNT = 128

KFS_SUPERBLOCK_BLK = 0
KFS_BLOCK_BITMAP_START = 1
KFS_BLOCK_BITMAP_BLKS = 8
KFS_INODE_BITMAP_START = 9
KFS_INODE_BITMAP_BLKS = 1
KFS_INODE_TABLE_START = 10
KFS_INODE_TABLE_BLKS = 16
KFS_DATA_BLOCKS_START = 26

KFS_ROOT_INODE = 0

KFS_TYPE_FREE = 0
KFS_TYPE_FILE = 1
KFS_TYPE_DIR = 2

class KFS:
    def __init__(self):
        self.blocks = bytearray(KFS_TOTAL_BLOCKS * KFS_BLOCK_SIZE)
        
        # Init superblock
        sb = struct.pack("<11I468x", 
            KFS_MAGIC, KFS_TOTAL_BLOCKS, KFS_INODE_COUNT,
            KFS_BLOCK_BITMAP_START, KFS_BLOCK_BITMAP_BLKS,
            KFS_INODE_BITMAP_START, KFS_INODE_BITMAP_BLKS,
            KFS_INODE_TABLE_START, KFS_INODE_TABLE_BLKS,
            KFS_DATA_BLOCKS_START, KFS_ROOT_INODE
        )
        assert len(sb) == 512
        self.write_block(KFS_SUPERBLOCK_BLK, sb)
        
        # Mark metadata blocks as used (0 to 25)
        for i in range(KFS_DATA_BLOCKS_START):
            self.set_block_bitmap(i, True)
            
    def write_block(self, blk, data):
        assert len(data) <= KFS_BLOCK_SIZE
        if len(data) < KFS_BLOCK_SIZE:
            data = data.ljust(KFS_BLOCK_SIZE, b'\0')
        start = blk * KFS_BLOCK_SIZE
        self.blocks[start:start+KFS_BLOCK_SIZE] = data
        
    def set_block_bitmap(self, blk, used):
        bitmap_byte = (blk // 8)
        bitmap_bit = blk % 8
        offset = KFS_BLOCK_BITMAP_START * KFS_BLOCK_SIZE + bitmap_byte
        if used:
            self.blocks[offset] |= (1 << bitmap_bit)
        else:
            self.blocks[offset] &= ~(1 << bitmap_bit)
            
    def set_inode_bitmap(self, ino, used):
        bitmap_byte = (ino // 8)
        bitmap_bit = ino % 8
        offset = KFS_INODE_BITMAP_START * KFS_BLOCK_SIZE + bitmap_byte
        if used:
            self.blocks[offset] |= (1 << bitmap_bit)
        else:
            self.blocks[offset] &= ~(1 << bitmap_bit)

    def write_inode(self, ino, size, typ, direct, indirect):
        assert len(direct) == 10
        inode_data = struct.pack("<14I8x", size, typ, *direct, indirect, 1)
        assert len(inode_data) == 64
        
        offset = KFS_INODE_TABLE_START * KFS_BLOCK_SIZE + (ino * 64)
        self.blocks[offset:offset+64] = inode_data
        self.set_inode_bitmap(ino, True)

    def allocate_block(self):
        # find free block
        for i in range(KFS_DATA_BLOCKS_START, KFS_TOTAL_BLOCKS):
            bitmap_byte = (i // 8)
            bitmap_bit = i % 8
            offset = KFS_BLOCK_BITMAP_START * KFS_BLOCK_SIZE + bitmap_byte
            if not (self.blocks[offset] & (1 << bitmap_bit)):
                self.set_block_bitmap(i, True)
                return i
        raise Exception("Out of blocks")

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <output.img> <file1> <file2> ...")
        sys.exit(1)
        
    out_file = sys.argv[1]
    input_files = sys.argv[2:]
    
    kfs = KFS()
    
    next_inode = KFS_ROOT_INODE + 1
    
    root_dirents = bytearray()
    
    print(f"KFS Formatting {out_file}:")
    print(f"  blocks:       {KFS_TOTAL_BLOCKS}")
    print(f"  inodes:       {KFS_INODE_COUNT}")
    print(f"  data start:   {KFS_DATA_BLOCKS_START}")
    print(f"  files:")
    
    for path in input_files:
        name = os.path.basename(path)
        with open(path, "rb") as f:
            data = f.read()
            
        size = len(data)
        ino = next_inode
        next_inode += 1
        
        if ino >= KFS_INODE_COUNT:
            raise Exception("Out of inodes")
            
        print(f"    /{name:<15} ino: {ino:<3} size: {size}")
        
        # Allocate blocks
        blocks = []
        for i in range(0, size, KFS_BLOCK_SIZE):
            blk = kfs.allocate_block()
            chunk = data[i:i+KFS_BLOCK_SIZE]
            kfs.write_block(blk, chunk)
            blocks.append(blk)
            
        direct = [0] * 10
        indirect = 0
        
        for i in range(min(10, len(blocks))):
            direct[i] = blocks[i]
            
        if len(blocks) > 10:
            indirect = kfs.allocate_block()
            indirect_data = struct.pack(f"<{len(blocks)-10}I", *blocks[10:])
            kfs.write_block(indirect, indirect_data)
            
        kfs.write_inode(ino, size, KFS_TYPE_FILE, direct, indirect)
        
        # Add to root dirents
        name_bytes = name.encode('ascii') + b'\0'
        # calculate padded reclen (align to 4)
        base_len = 4 + 2 + 1 + len(name_bytes) # inode(4) + reclen(2) + type(1) + name
        reclen = (base_len + 3) & ~3
        pad = reclen - base_len
        
        dirent = struct.pack("<IHBB", ino, reclen, KFS_TYPE_FILE, 0) # padding type byte
        dirent = struct.pack("<IHB", ino, reclen, KFS_TYPE_FILE) + name_bytes + (b'\0' * pad)
        assert len(dirent) == reclen
        
        root_dirents += dirent
        
    # Write root directory inode
    print(f"    / (root)        ino: {KFS_ROOT_INODE:<3} size: {len(root_dirents)}")
    blocks = []
    for i in range(0, len(root_dirents), KFS_BLOCK_SIZE):
        blk = kfs.allocate_block()
        chunk = root_dirents[i:i+KFS_BLOCK_SIZE]
        kfs.write_block(blk, chunk)
        blocks.append(blk)
        
    direct = [0] * 10
    indirect = 0
    for i in range(min(10, len(blocks))):
        direct[i] = blocks[i]
    if len(blocks) > 10:
        indirect = kfs.allocate_block()
        indirect_data = struct.pack(f"<{len(blocks)-10}I", *blocks[10:])
        kfs.write_block(indirect, indirect_data)
        
    kfs.write_inode(KFS_ROOT_INODE, len(root_dirents), KFS_TYPE_DIR, direct, indirect)
    
    with open(out_file, "wb") as f:
        f.write(kfs.blocks)

if __name__ == "__main__":
    main()
