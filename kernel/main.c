/*
 * main.c — Kernel entry point
 *
 * Initialization sequence:
 *   1. UART (serial console)
 *   2. Exception vector table
 *   3. GIC (interrupt controller)
 *   4. Physical memory manager
 *   5. Virtual memory (MMU)
 *   6. Kernel heap allocator
 *   7. Timer (periodic tick)
 */

#include "kernel.h"
#include "drivers/uart.h"
#include "drivers/gic.h"
#include "drivers/timer.h"
#include "mm.h"

/* Defined in vectors.S */
extern void *exception_vector_table;

/* Install the exception vector table */
static inline void exception_init(void) {
    uint64_t addr = (uint64_t)&exception_vector_table;
    asm volatile("msr vbar_el1, %0" :: "r"(addr));
    asm volatile("isb");
    kprintf("[exception] Vector table installed at %p\n", (void *)addr);
}

/* Enable IRQs and FIQs */
static inline void irq_enable(void) {
    asm volatile("msr daifclr, #0x3");
}

void kernel_main(void) {
    /* Phase 1: UART */
    uart_init();

    kprintf("=================================\n");
    kprintf("  ArmOS Kernel v0.3.0\n");
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

    /* Phase 3: Memory */
    pmm_init();

    uint64_t *kernel_pt = vmm_create_kernel_tables();
    vmm_enable_mmu(kernel_pt);

    kmalloc_init();

    /* Quick sanity test: allocate and free */
    void *test1 = kmalloc(64);
    void *test2 = kmalloc(128);
    kprintf("[test] kmalloc(64)  = %p\n", test1);
    kprintf("[test] kmalloc(128) = %p\n", test2);
    kfree(test1);
    kfree(test2);
    kprintf("[test] kfree OK — heap used: %lu bytes\n\n", kmalloc_used());

    /* Timer */
    timer_init();

    kprintf("\n[kernel] Enabling interrupts...\n");
    irq_enable();

    kprintf("[kernel] Boot complete. Waiting for interrupts...\n\n");

    /* Idle loop */
    while (1) {
        asm volatile("wfi");
    }
}
