#ifndef KRYOS_IDT_H
#define KRYOS_IDT_H

#include <stdint.h>

// IDT Entry (16 bytes for x86_64)
typedef struct {
    uint16_t offset_low;        // Offset bits 0..15
    uint16_t selector;          // Code segment selector in GDT
    uint8_t  ist;               // Interrupt Stack Table offset (usually 0)
    uint8_t  type_attributes;   // Gate type, DPL, and Present fields
    uint16_t offset_mid;        // Offset bits 16..31
    uint32_t offset_high;       // Offset bits 32..63
    uint32_t reserved;          // Reserved, set to 0
} __attribute__((packed)) idt_entry_t;

// IDTR (Pointer to IDT array)
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

#endif
