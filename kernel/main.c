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

static inline void exception_init(void) {
    uint64_t addr = (uint64_t)&exception_vector_table;
    asm volatile("msr vbar_el1, %0" :: "r"(addr));
    asm volatile("isb");
    kprintf("[exception] Vector table installed at %p\n", (void *)addr);
}

static inline void irq_enable(void) {
    asm volatile("msr daifclr, #0x3");
}

/* ─── Test kernel threads ─── */

static void task_a(void) {
    while (1) {
        kprintf("[task-A] Hello from Task A (PID %lu)\n", proc_current()->pid);
        yield();
    }
}

static void task_b(void) {
    while (1) {
        kprintf("[task-B] Hello from Task B (PID %lu)\n", proc_current()->pid);
        yield();
    }
}

static void task_c(void) {
    int count = 0;
    while (1) {
        kprintf("[task-C] Count = %d (PID %lu)\n", count++, proc_current()->pid);
        yield();
    }
}


void kernel_main(void) {
    uart_init();

    kprintf("=================================\n");
    kprintf("  ArmOS Kernel v0.4.0\n");
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

    process_t *pa = proc_create_kthread("task-A", task_a);
    process_t *pb = proc_create_kthread("task-B", task_b);
    process_t *pc = proc_create_kthread("task-C", task_c);
    sched_enqueue(pa);
    sched_enqueue(pb);
    sched_enqueue(pc);

    timer_init();

    kprintf("\n[kernel] Enabling interrupts...\n");
    irq_enable();
    kprintf("[kernel] Boot complete. 3 tasks scheduled.\n\n");

    /* Idle loop — yield to let tasks run */
    while (1) {
        yield();
    }
}
