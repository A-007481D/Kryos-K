#include "../../include/irq.h"
#include "../../include/stdio.h"
#include "../../include/thread.h"
#include "pic.h"
#include <stddef.h>

static irq_handler_t irq_routines[16] = {0};

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_routines[irq] = handler;
    }
}

void irq_dispatch(kernel_interrupt_frame *frame) {
    if (frame->int_no >= 32 && frame->int_no <= 47) {
        uint8_t irq = frame->int_no - 32;
        
        // Send EOI early because the handler might call schedule() and not return immediately.
        // Since IF=0 during the interrupt handler, this won't cause nested interrupts.
        pic_eoi(irq);
        
        irq_handler_t handler = irq_routines[irq];
        if (handler) {
            handler(frame);
        } else if (irq == 0) {
            timer_handler();
        } else {
            kprintf("Unhandled IRQ %d\n", irq);
        }
    }
}
