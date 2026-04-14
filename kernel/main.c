/*
 * main.c — Kernel entry point
 */

#include "kernel.h"
#include "drivers/uart.h"
#include "drivers/gic.h"
#include "drivers/timer.h"
#include "mm.h"
#include "proc.h"

extern void *exception_vector_table;

/* Defined in user/init.c — linked into the kernel image for now */
extern void user_main(void);

/* Second demo kthread — prints a few messages and exits, showing
 * cooperative multitasking between two runnable tasks. */
static void worker_main(void) {
    for (int i = 0; i < 5; i++) {
        kprintf("[worker] tick %d\n", i);
        yield();
    }
    kprintf("[worker] done\n");
}

static inline void exception_init(void) {
    uint64_t addr = (uint64_t)&exception_vector_table;
    asm volatile("msr vbar_el1, %0" :: "r"(addr));
    asm volatile("isb");
    kprintf("[exception] Vector table installed at %p\n", (void *)addr);
}

static inline void irq_enable(void) {
    asm volatile("msr daifclr, #0x3");
}


void kernel_main(void) {
    uart_init();

    kprintf("=================================\n");
    kprintf("  ArmOS Kernel v0.5.0\n");
    kprintf("  AArch64 | QEMU virt machine\n");
    kprintf("=================================\n\n");

    extern uint8_t __bss_start, __bss_end, _stack_top, __kernel_end;
    kprintf("[boot] BSS:        %p - %p\n", &__bss_start, &__bss_end);
    kprintf("[boot] Stack top:  %p\n", &_stack_top);
    kprintf("[boot] Kernel end: %p\n\n", &__kernel_end);

    exception_init();
    gic_init();
    pmm_init();

    uint64_t *kernel_pt = vmm_create_kernel_tables();
    vmm_enable_mmu(kernel_pt);
    kmalloc_init();

    proc_init();
    sched_init();

    /* Create kernel threads — user-space SVC path is deferred */
    process_t *init = proc_create_kthread("init", user_main);
    process_t *worker = proc_create_kthread("worker", worker_main);
    sched_enqueue(init);
    sched_enqueue(worker);

    timer_init();

    kprintf("\n[kernel] Enabling interrupts...\n");
    irq_enable();
    kprintf("[kernel] Boot complete. Scheduling tasks.\n\n");

    /* Idle loop */
    while (1) {
        yield();
    }
}
