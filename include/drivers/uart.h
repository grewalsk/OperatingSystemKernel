#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

#include "kernel.h"

/*
 * PL011 UART Driver
 *
 * The PL011 is ARM's standard UART controller. On QEMU virt,
 * it's mapped at physical address 0x09000000.
 *
 * We use it as our sole I/O device — serial console for all
 * kernel output and (later) user input.
 */

/* Initialize the UART hardware */
void uart_init(void);

/* Send a single character */
void uart_putc(char c);

/* Send a null-terminated string */
void uart_puts(const char *s);

/* Read a single character (blocking) */
char uart_getc(void);

/* Kernel printf — formatted output to UART */
void kprintf(const char *fmt, ...);

#endif /* DRIVERS_UART_H */
