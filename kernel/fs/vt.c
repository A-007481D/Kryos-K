#include "vt.h"
#include "../../include/serial.h"

struct vt vts[MAX_VTS];
static int active_vt_idx = 0;

static void vt_clear(int vt_num) {
    struct vt *v = &vts[vt_num];
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        v->buffer[i] = ((uint16_t)v->current_attr << 8) | ' ';
    }
    v->cursor_x = 0;
    v->cursor_y = 0;
}

static void vt_scroll(int vt_num) {
    struct vt *v = &vts[vt_num];
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            v->buffer[(y - 1) * VGA_WIDTH + x] = v->buffer[y * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        v->buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = ((uint16_t)v->current_attr << 8) | ' ';
    }
    v->cursor_y = VGA_HEIGHT - 1;
}

static void vt_sync_cursor(int vt_num) {
    if (vts[vt_num].active) {
        vga_set_cursor(vts[vt_num].cursor_x, vts[vt_num].cursor_y);
    }
}

static void vt_putc(int vt_num, char c) {
    struct vt *v = &vts[vt_num];

    if (c == '\n') {
        v->cursor_x = 0;
        v->cursor_y++;
    } else if (c == '\b') {
        if (v->cursor_x > 0) {
            v->cursor_x--;
        } else if (v->cursor_y > 0) {
            v->cursor_y--;
            v->cursor_x = VGA_WIDTH - 1;
        }
        v->buffer[v->cursor_y * VGA_WIDTH + v->cursor_x] = ((uint16_t)v->current_attr << 8) | ' ';
    } else if (c == '\r') {
        v->cursor_x = 0;
    } else {
        v->buffer[v->cursor_y * VGA_WIDTH + v->cursor_x] = ((uint16_t)v->current_attr << 8) | (uint8_t)c;
        if (v->active) {
            vga_putc(v->cursor_x, v->cursor_y, c, v->current_attr);
        }
        v->cursor_x++;
    }

    if (v->cursor_x >= VGA_WIDTH) {
        v->cursor_x = 0;
        v->cursor_y++;
    }

    if (v->cursor_y >= VGA_HEIGHT) {
        vt_scroll(vt_num);
        if (v->active) {
            vga_blit(v->buffer);
        }
    }
    vt_sync_cursor(vt_num);
}

void vt_init(void) {
    vga_init();
    for (int i = 0; i < MAX_VTS; i++) {
        vts[i].current_attr = VGA_COLOR_LIGHT_GREY | (VGA_COLOR_BLACK << 4);
        vts[i].active = (i == 0);
        vt_clear(i);
        vts[i].ansi_state = 0;
    }
    vga_blit(vts[0].buffer);
    vga_set_cursor(0, 0);
}

void vt_switch(int vt_num) {
    if (vt_num < 0 || vt_num >= MAX_VTS) return;
    if (vt_num == active_vt_idx) return;

    vts[active_vt_idx].active = 0;
    active_vt_idx = vt_num;
    vts[active_vt_idx].active = 1;

    vga_blit(vts[active_vt_idx].buffer);
    vga_set_cursor(vts[active_vt_idx].cursor_x, vts[active_vt_idx].cursor_y);
}

int vt_get_active(void) {
    return active_vt_idx;
}

// Minimal ANSI escape sequence parser
// States: 0 = normal, 1 = \x1b seen, 2 = [ seen, 3 = param1 parsing, 4 = param2 parsing
static void vt_parse_ansi(int vt_num, char c) {
    struct vt *v = &vts[vt_num];

    if (v->ansi_state == 0) {
        if (c == 0x1B) {
            v->ansi_state = 1;
            v->ansi_param = 0;
            v->ansi_param2 = 0;
        } else {
            vt_putc(vt_num, c);
        }
    } else if (v->ansi_state == 1) {
        if (c == '[') {
            v->ansi_state = 2;
        } else {
            // Abort
            v->ansi_state = 0;
            vt_putc(vt_num, c);
        }
    } else if (v->ansi_state == 2 || v->ansi_state == 3 || v->ansi_state == 4) {
        if (c >= '0' && c <= '9') {
            if (v->ansi_state == 2) v->ansi_state = 3;
            if (v->ansi_state == 3) {
                v->ansi_param = v->ansi_param * 10 + (c - '0');
            } else if (v->ansi_state == 4) {
                v->ansi_param2 = v->ansi_param2 * 10 + (c - '0');
            }
        } else if (c == ';') {
            v->ansi_state = 4;
        } else if (c == 'm') {
            // Colors
            // Very simplified: 0 = reset, 3x = fg color
            if (v->ansi_param == 0) {
                v->current_attr = VGA_COLOR_LIGHT_GREY | (VGA_COLOR_BLACK << 4);
            } else if (v->ansi_param >= 30 && v->ansi_param <= 37) {
                // Approximate mapping to VGA colors
                uint8_t fg = v->ansi_param - 30;
                // ansi: 0=blk, 1=red, 2=grn, 3=yel(brn), 4=blu, 5=mag, 6=cyn, 7=wht
                uint8_t mapping[] = {0, 4, 2, 6, 1, 5, 3, 7}; 
                fg = mapping[fg];
                v->current_attr = (v->current_attr & 0xF0) | fg;
            }
            v->ansi_state = 0;
        } else if (c == 'J') {
            if (v->ansi_param == 2) {
                vt_clear(vt_num);
                if (v->active) {
                    vga_blit(v->buffer);
                }
            }
            v->ansi_state = 0;
        } else if (c == 'H') {
            int x = v->ansi_param2 > 0 ? v->ansi_param2 - 1 : 0;
            int y = v->ansi_param > 0 ? v->ansi_param - 1 : 0;
            if (x < 0) x = 0; 
            if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
            if (y < 0) y = 0; 
            if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
            v->cursor_x = x;
            v->cursor_y = y;
            vt_sync_cursor(vt_num);
            v->ansi_state = 0;
        } else {
            // Unsupported
            v->ansi_state = 0;
        }
    }
}

void vt_write(int vt_num, const char *data, size_t len) {
    if (vt_num < 0 || vt_num >= MAX_VTS) return;
    
    // Also echo to serial unconditionally for automation
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\b') {
            serial_puts("\b \b");
        } else {
            char s[2] = {data[i], 0};
            serial_puts(s);
        }
        vt_parse_ansi(vt_num, data[i]);
    }
}
