#include "ata.h"
#include "../../include/blk.h"
#include <stddef.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ( "inw %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ( "outw %0, %1" : : "a"(val), "Nd"(port) );
}



static int ata_wait_ready(void) {
    while (1) {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DF) return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
    }
    return -1;
}

static int ata_wait_busy(void) {
    while (1) {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DF) return -1;
        if (!(status & ATA_SR_BSY)) return 0;
    }
    return -1;
}

static int ata_read_sectors_impl(struct blk_dev *dev, uint64_t lba, uint32_t count, void *buf) {
    (void)dev;
    uint16_t *ptr = (uint16_t *)buf;

    while (count > 0) {
        outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_PRIMARY_IO + ATA_REG_FEATURES, 0x00);
        outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 1);
        outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (uint8_t)lba);
        outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (uint8_t)(lba >> 16));
        outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        if (ata_wait_ready() != 0) return -1;

        for (int i = 0; i < 256; i++) {
            ptr[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
        }

        ptr += 256;
        lba++;
        count--;
    }
    return 0;
}

static int ata_write_sectors_impl(struct blk_dev *dev, uint64_t lba, uint32_t count, const void *buf) {
    (void)dev;
    const uint16_t *ptr = (const uint16_t *)buf;

    while (count > 0) {
        outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_PRIMARY_IO + ATA_REG_FEATURES, 0x00);
        outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 1);
        outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (uint8_t)lba);
        outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (uint8_t)(lba >> 16));
        outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

        // Wait for BSY to clear and DRQ to set
        if (ata_wait_ready() != 0) return -1;

        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_IO + ATA_REG_DATA, ptr[i]);
        }

        // Flush cache? Wait for busy to clear
        if (ata_wait_busy() != 0) return -1;

        ptr += 256;
        lba++;
        count--;
    }
    return 0;
}

static struct blk_dev hda_dev = {
    .name = "hda",
    .sector_count = 0,
    .read_sectors = ata_read_sectors_impl,
    .write_sectors = ata_write_sectors_impl,
    .priv = NULL
};

void ata_init(void) {
    extern void serial_puts(const char*);
    extern void kprintf(const char *fmt, ...);
    
    serial_puts("ATA: initializing...\n");
    
    // Select master drive (0xA0)
    outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xA0);
    // Set sector count and LBA regs to 0
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA0, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA1, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA2, 0);
    
    // Send IDENTIFY
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status == 0) {
        serial_puts("ATA: Status 0, no drive\n");
        return;
    }
    
    if (ata_wait_ready() != 0) {
        serial_puts("ATA: Wait ready failed\n");
        return;
    }
    
    uint16_t identify[256] = {0};
    for (int i = 0; i < 256; i++) {
        identify[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }
    
    uint32_t sectors_28 = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
    
    if (sectors_28 > 0) {
        hda_dev.sector_count = sectors_28;
        blk_register(&hda_dev);
        kprintf("ATA: Registered hda (%d sectors)\n", (int)sectors_28);
    } else {
        serial_puts("ATA: Sectors is 0\n");
    }
}
