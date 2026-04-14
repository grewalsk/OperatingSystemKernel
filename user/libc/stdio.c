/*
 * stdio.c — Minimal user-space stdio
 *
 * These functions use system calls to communicate with the kernel.
 */

#include <stdint.h>

/* Syscall stubs (defined in syscall.S) */
extern int64_t sys_write(int fd, const void *buf, uint64_t count);

static uint64_t strlen(const char *s) {
    uint64_t len = 0;
    while (s[len]) len++;
    return len;
}

void puts(const char *s) {
    sys_write(1, s, strlen(s));
    sys_write(1, "\n", 1);
}

/* Minimal putchar */
void putchar(char c) {
    sys_write(1, &c, 1);
}

/* Minimal integer to string */
static void print_int(int64_t val) {
    if (val < 0) {
        putchar('-');
        val = -val;
    }
    char buf[20];
    int i = 0;
    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val > 0) {
            buf[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (--i >= 0) {
        putchar(buf[i]);
    }
}

/* Very basic printf — supports %s, %d, %c */
void printf(const char *fmt, ...) {
    /* Use built-in va_list */
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            putchar(*fmt);
            fmt++;
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 's': {
            const char *s = __builtin_va_arg(args, const char *);
            if (!s) s = "(null)";
            sys_write(1, s, strlen(s));
            break;
        }
        case 'd': {
            int64_t val = __builtin_va_arg(args, int64_t);
            print_int(val);
            break;
        }
        case 'c': {
            char c = (char)__builtin_va_arg(args, int);
            putchar(c);
            break;
        }
        case '%':
            putchar('%');
            break;
        default:
            putchar('%');
            putchar(*fmt);
            break;
        }
        fmt++;
    }

    __builtin_va_end(args);
}
