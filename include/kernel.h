#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* Boolean type — use stdbool if available (GCC 15+), otherwise define manually */
#include <stdbool.h>

/* Common kernel macros */
#define UNUSED(x) ((void)(x))

/* Kernel panic — prints message and halts */
void panic(const char *fmt, ...);

#endif /* KERNEL_H */
