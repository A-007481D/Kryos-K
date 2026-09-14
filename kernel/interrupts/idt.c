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

extern uint64_t isr_stub_table[];

void idt_init(void) {
    idtr.base = (uint64_t)&idt;
    idtr.limit = sizeof(idt) - 1;

    for (int i = 0; i < 32; i++) {
        // 0x08 is the 64-bit code segment from our GDT
        // 0x8E = Present (1) | DPL (00) | Storage (0) | Gate Type (1110)
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);
    }

    // Load IDT
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
