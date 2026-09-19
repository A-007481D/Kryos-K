#include "ps2.h"
#include "../../include/irq.h"
#include "../../include/stdio.h"
#include "../fs/tty.h"
#include "../fs/vt.h"
#include <stdint.h>
#include <stdbool.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Scancode Set 1 decoding
static const char scancode_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char scancode_ascii_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

static bool shift_pressed = false;
static bool alt_pressed = false;
static bool extended_mode = false;

// Raw buffer
#define RAW_BUF_SIZE 256
static uint8_t raw_buffer[RAW_BUF_SIZE];
static int raw_head = 0;
static int raw_tail = 0;

static void keyboard_irq_handler(kernel_interrupt_frame *frame) {
    (void)frame;
    
    // Read status
    uint8_t status = inb(PS2_STATUS_PORT);
    if (!(status & 1)) {
        return; // No data available
    }
    
    uint8_t scancode = inb(PS2_DATA_PORT);
    
    // Store in raw diagnostic buffer
    raw_buffer[raw_head] = scancode;
    raw_head = (raw_head + 1) % RAW_BUF_SIZE;
    if (raw_head == raw_tail) {
        raw_tail = (raw_tail + 1) % RAW_BUF_SIZE; // overflow
    }
    
    if (scancode == 0xE0) {
        extended_mode = true;
        return;
    }
    
    if (extended_mode) {
        extended_mode = false;
        // Ignore most extended keys for now, but handle extended Alt if needed
        // Actually left Alt is 0x38 without E0, right Alt is E0 38.
        if (scancode == 0x38) alt_pressed = true;
        else if (scancode == 0xB8) alt_pressed = false;
        return;
    }
    
    if (scancode & 0x80) {
        // Break code
        uint8_t make_code = scancode & 0x7F;
        if (make_code == 0x2A || make_code == 0x36) { // Left or Right Shift
            shift_pressed = false;
        } else if (make_code == 0x38) { // Alt
            alt_pressed = false;
        }
    } else {
        // Make code
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
        } else if (scancode == 0x38) {
            alt_pressed = true;
        } else if (alt_pressed && scancode >= 0x3B && scancode <= 0x3E) {
            // Alt + F1..F4
            int vt_num = scancode - 0x3B;
            vt_switch(vt_num);
        } else if (scancode < sizeof(scancode_ascii)) {
            char c = shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];
            if (c) {
                tty_receive_char(c);
            }
        }
    }
}

void ps2_init(void) {
    irq_register_handler(1, keyboard_irq_handler);
}
