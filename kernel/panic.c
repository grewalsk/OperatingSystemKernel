/*
 * panic.c — Kernel panic handler
 *
 * Called when something goes catastrophically wrong.
 * Prints the message and halts the CPU forever.
 */

#include "kernel.h"
#include "drivers/uart.h"

void panic(const char *fmt, ...) {
    kprintf("\n!!! KERNEL PANIC !!!\n");

    /* Reuse kprintf for the panic message by forwarding variadic args.
     * For simplicity in Phase 1, just print the format string directly.
     * We'll add full variadic forwarding when we need it. */
    uart_puts(fmt);
    uart_puts("\n");

    kprintf("System halted.\n");

    /* Disable interrupts and halt forever */
    while (1) {
        asm volatile("msr daifset, #0xF");  /* Mask all interrupts */
        asm volatile("wfe");
    }
}
