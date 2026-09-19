#include "vga.h"
#include "../../kernel/memory/virt.h"

#define VGA_CMD_PORT  0x3D4
#define VGA_DATA_PORT 0x3D5

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint16_t *vga_buffer;

void vga_init(void) {
    vga_buffer = (uint16_t *)phys_to_virt(0xB8000);
}

void vga_set_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;

    outb(VGA_CMD_PORT, 0x0F);
    outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFF));
    outb(VGA_CMD_PORT, 0x0E);
    outb(VGA_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_putc(int x, int y, char c, uint8_t color) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        int index = y * VGA_WIDTH + x;
        vga_buffer[index] = ((uint16_t)color << 8) | (uint8_t)c;
    }
}

void vga_blit(const uint16_t *buffer) {
    if (!vga_buffer || !buffer) return;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = buffer[i];
    }
}
