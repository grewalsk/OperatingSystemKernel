/*
 * main.c — Kernel entry point
 *
 * Initialization sequence:
 *   1. UART (serial console — so we can see output)
 *   2. Exception vector table (so we can handle interrupts)
 *   3. GIC (interrupt controller — routes hardware IRQs)
 *   4. Timer (periodic tick — drives scheduling later)
 */

#include "kernel.h"
#include "drivers/uart.h"
#include "drivers/gic.h"
#include "drivers/timer.h"

/* Defined in exception.S */
extern void *exception_vector_table;

/* Install the exception vector table */
static inline void exception_init(void) {
    uint64_t addr = (uint64_t)&exception_vector_table;
    asm volatile("msr vbar_el1, %0" :: "r"(addr));
    asm volatile("isb");
    kprintf("[exception] Vector table installed at %p\n", (void *)addr);
}

/* Enable IRQs and FIQs (clear DAIF interrupt masks) */
static inline void irq_enable(void) {
    asm volatile("msr daifclr, #0x3");  /* Clear I and F bits */
}

void kernel_main(void) {
    /* Phase 1: UART */
    uart_init();

    kprintf("=================================\n");
    kprintf("  ArmOS Kernel v0.2.0\n");
    kprintf("  AArch64 | QEMU virt machine\n");
    kprintf("=================================\n\n");

    /* Print boot info */
    extern uint8_t __bss_start, __bss_end, _stack_top, __kernel_end;
    kprintf("[boot] BSS:        %p - %p\n", &__bss_start, &__bss_end);
    kprintf("[boot] Stack top:  %p\n", &_stack_top);
    kprintf("[boot] Kernel end: %p\n\n", &__kernel_end);

    /* Phase 2: Interrupts */
    exception_init();
    gic_init();
    timer_init();

    kprintf("\n[kernel] Enabling interrupts...\n");
    irq_enable();

    kprintf("[kernel] Boot complete. Waiting for interrupts...\n\n");

    /* Idle loop — timer interrupts will fire and print tick counts */
    while (1) {
        asm volatile("wfi");    /* Wait For Interrupt (low-power idle) */
    }
}
