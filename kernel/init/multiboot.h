#pragma once

#include <stdint.h>

void multiboot_parse(uint32_t magic, uint32_t info_addr_phys);
