#ifndef KRYOS_FS_VT_H
#define KRYOS_FS_VT_H

#include <stdint.h>
#include <stddef.h>
#include "../drivers/vga.h"

#define MAX_VTS 4

struct vt {
    uint16_t buffer[VGA_WIDTH * VGA_HEIGHT];
    int cursor_x;
    int cursor_y;
    uint8_t current_attr;
    
    // ANSI parser state
    int ansi_state;
    int ansi_param;
    int ansi_param2;
    int active; // 1 if this VT is currently shown on VGA
};

void vt_init(void);
void vt_switch(int vt_num);
int vt_get_active(void);
void vt_write(int vt_num, const char *data, size_t len);

#endif
