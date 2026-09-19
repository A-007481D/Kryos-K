#pragma once

#include "interrupts.h"

typedef void (*irq_handler_t)(kernel_interrupt_frame *frame);

void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_dispatch(kernel_interrupt_frame *frame);
