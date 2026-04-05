/*
 * main.c — Kernel entry point
 *
 * This is where C code begins after boot.S sets up the CPU.
 * Currently just initializes the UART and prints a hello message.
 * Future phases will add interrupt setup, memory management,
 * process scheduling, and more.
 */

#include "kernel.h"
#include "drivers/uart.h"

void kernel_main(void) {
    /* Initialize hardware */
    uart_init();

    /* We're alive! */
    kprintf("=================================\n");
    kprintf("  ArmOS Kernel v0.1.0\n");
    kprintf("  AArch64 | QEMU virt machine\n");
    kprintf("=================================\n");
    kprintf("\n");
    kprintf("Hello, kernel!\n");
    kprintf("\n");

    /* Print some boot info */
    extern uint8_t __bss_start, __bss_end, _stack_top, __kernel_end;
    kprintf("[boot] BSS:        %p - %p\n", &__bss_start, &__bss_end);
    kprintf("[boot] Stack top:  %p\n", &_stack_top);
    kprintf("[boot] Kernel end: %p\n", &__kernel_end);
    kprintf("\n");
    kprintf("[kernel] Boot complete. Halting.\n");

    /* Halt — nothing else to do yet */
    while (1) {
        asm volatile("wfe");
    }
}
