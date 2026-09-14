#include "idt.h"

static idt_entry_t idt[256];
static idtr_t idtr;

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attributes = flags;
    idt[num].offset_mid = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].reserved = 0;
}

void idt_init(void) {
    idtr.base = (uint64_t)&idt;
    idtr.limit = sizeof(idt) - 1;

    // We will bind ISRs here in the next commit

    // Load IDT
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
