#pragma once

#include <stdint.h>

void multiboot_parse(uint32_t magic, uint32_t info_addr_phys);
bool multiboot_get_module(uint32_t index, uint64_t *start_phys, uint64_t *end_phys);
