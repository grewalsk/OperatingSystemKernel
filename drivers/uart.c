/*
 * uart.c — PL011 UART driver for QEMU virt machine
 *
 * The PL011 is a memory-mapped peripheral. We interact with it by
 * reading/writing specific addresses (registers). Each register
 * controls a different aspect of the UART hardware.
 *
 * Base address on QEMU virt: 0x09000000
 *
 * Key registers (offsets from base):
 *   DR   (0x000) — Data Register: write to send, read to receive
 *   FR   (0x018) — Flag Register: status bits (TX full, RX empty, etc.)
 *   IBRD (0x024) — Integer Baud Rate Divisor
 *   FBRD (0x028) — Fractional Baud Rate Divisor
 *   LCR  (0x02C) — Line Control Register (word length, parity, etc.)
 *   CR   (0x030) — Control Register (enable UART, TX, RX)
 *   ICR  (0x044) — Interrupt Clear Register
 */

#include "drivers/uart.h"

/* PL011 base address on QEMU virt machine */
#define UART_BASE       0x09000000UL

/* Register offsets */
#define UART_DR         (*(volatile uint32_t *)(UART_BASE + 0x000))
#define UART_FR         (*(volatile uint32_t *)(UART_BASE + 0x018))
#define UART_IBRD       (*(volatile uint32_t *)(UART_BASE + 0x024))
#define UART_FBRD       (*(volatile uint32_t *)(UART_BASE + 0x028))
#define UART_LCR_H      (*(volatile uint32_t *)(UART_BASE + 0x02C))
#define UART_CR         (*(volatile uint32_t *)(UART_BASE + 0x030))
#define UART_ICR        (*(volatile uint32_t *)(UART_BASE + 0x044))

/* Flag Register bits */
#define FR_TXFF         (1 << 5)    /* Transmit FIFO full */
#define FR_RXFE         (1 << 4)    /* Receive FIFO empty */
#define FR_BUSY         (1 << 3)    /* UART busy transmitting */

/* Line Control Register bits */
#define LCR_FEN         (1 << 4)    /* Enable FIFOs */
#define LCR_WLEN_8      (3 << 5)    /* 8-bit word length */

/* Control Register bits */
#define CR_UARTEN       (1 << 0)    /* UART enable */
#define CR_TXE          (1 << 8)    /* Transmit enable */
#define CR_RXE          (1 << 9)    /* Receive enable */


void uart_init(void) {
    /* Disable UART while configuring */
    UART_CR = 0;

    /* Clear all pending interrupts */
    UART_ICR = 0x7FF;

    /*
     * Set baud rate.
     * QEMU doesn't actually care about baud rate (it's emulated),
     * but we configure it properly for correctness.
     *
     * Assuming 24 MHz UART clock (QEMU virt default):
     *   Baud rate divisor = 24000000 / (16 * 115200) = 13.0208
     *   Integer part: 13
     *   Fractional part: 0.0208 * 64 = 1.33 ≈ 1
     */
    UART_IBRD = 13;
    UART_FBRD = 1;

    /*
     * Configure line control:
     *   - 8-bit word length (LCR_WLEN_8)
     *   - Enable FIFOs (LCR_FEN)
     *   - No parity, 1 stop bit (default)
     */
    UART_LCR_H = LCR_FEN | LCR_WLEN_8;

    /*
     * Enable UART, transmit, and receive.
     */
    UART_CR = CR_UARTEN | CR_TXE | CR_RXE;
}


void uart_putc(char c) {
    /* Wait until the transmit FIFO has space */
    while (UART_FR & FR_TXFF) {
        /* Spin — FIFO is full */
    }

    /* Write character to the data register */
    UART_DR = (uint32_t)c;
}


void uart_puts(const char *s) {
    while (*s) {
        /* Convert \n to \r\n for proper serial terminal behavior */
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s);
        s++;
    }
}


char uart_getc(void) {
    /* Wait until the receive FIFO has data */
    while (UART_FR & FR_RXFE) {
        /* Spin — FIFO is empty */
    }

    /* Read and return the character */
    return (char)(UART_DR & 0xFF);
}


/*
 * kprintf — minimal kernel printf
 *
 * Supports:
 *   %s  — string
 *   %c  — character
 *   %d  — signed decimal integer
 *   %u  — unsigned decimal integer
 *   %x  — unsigned hexadecimal (lowercase)
 *   %p  — pointer (prints as 0x...)
 *   %l  — long variants: %ld, %lu, %lx
 *   %%  — literal percent sign
 */

/* Helper: print unsigned integer in given base */
static void print_uint(uint64_t value, int base, int width) {
    char buf[20];
    int i = 0;

    if (value == 0) {
        buf[i++] = '0';
    } else {
        while (value > 0) {
            int digit = value % base;
            buf[i++] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
            value /= base;
        }
    }

    /* Pad with zeros if width specified */
    while (i < width) {
        buf[i++] = '0';
    }

    /* Print in reverse (most significant digit first) */
    while (--i >= 0) {
        uart_putc(buf[i]);
    }
}


void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            if (*fmt == '\n') {
                uart_putc('\r');
            }
            uart_putc(*fmt);
            fmt++;
            continue;
        }

        fmt++;  /* Skip '%' */

        /* Check for width specifier (simple: just leading zeros) */
        int width = 0;
        if (*fmt == '0') {
            fmt++;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        /* Check for 'l' (long) modifier */
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
        }

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            uart_puts(s);
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            uart_putc(c);
            break;
        }
        case 'd': {
            int64_t val;
            if (is_long) {
                val = va_arg(args, int64_t);
            } else {
                val = va_arg(args, int);
            }
            if (val < 0) {
                uart_putc('-');
                val = -val;
            }
            print_uint((uint64_t)val, 10, width);
            break;
        }
        case 'u': {
            uint64_t val;
            if (is_long) {
                val = va_arg(args, uint64_t);
            } else {
                val = va_arg(args, unsigned int);
            }
            print_uint(val, 10, width);
            break;
        }
        case 'x': {
            uint64_t val;
            if (is_long) {
                val = va_arg(args, uint64_t);
            } else {
                val = va_arg(args, unsigned int);
            }
            print_uint(val, 16, width);
            break;
        }
        case 'p': {
            void *ptr = va_arg(args, void *);
            uart_puts("0x");
            print_uint((uint64_t)ptr, 16, 16);
            break;
        }
        case '%':
            uart_putc('%');
            break;
        default:
            uart_putc('%');
            uart_putc(*fmt);
            break;
        }

        fmt++;
    }

    va_end(args);
}
