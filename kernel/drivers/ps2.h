#pragma once
#include <stdint.h>
#include <stdbool.h>

void ps2_init(void);
void ps2_handle_scancode(uint8_t scancode);
bool ps2_is_shift_pressed(void);
bool ps2_is_alt_pressed(void);
