/*
 * syscall.c — System call dispatch
 *
 * User programs invoke system calls via the SVC #0 instruction.
 * The CPU traps to EL1, and the exception handler calls us with
 * the saved register frame.
 *
 * ABI:
 *   x8  = syscall number
 *   x0-x5 = arguments
 *   x0  = return value
 */

#include "kernel.h"
#include "proc.h"
#include "drivers/uart.h"

/* System call numbers */
#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_YIELD   3
#define SYS_GETPID  4

/* Saved register frame from vectors.S (34 * 8 bytes on stack) */
typedef struct trap_frame {
    uint64_t regs[30];  /* x0-x29 */
    uint64_t lr;        /* x30 */
    uint64_t elr;       /* saved ELR_EL1 */
    uint64_t spsr;      /* saved SPSR_EL1 */
} trap_frame_t;


/*
 * sys_write — Write to console (fd=1 only for now)
 *
 * write(fd, buf, count) → bytes written
 */
static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count) {
    if (fd != 1 && fd != 2) {
        return -1;  /* Only stdout/stderr supported */
    }

    const char *str = (const char *)buf;
    for (uint64_t i = 0; i < count; i++) {
        uart_putc(str[i]);
    }
    return (int64_t)count;
}


/*
 * sys_exit — Terminate the current process
 */
static void sys_exit(int64_t status) {
    process_t *current = proc_current();
    kprintf("[syscall] Process '%s' (PID %lu) exited with status %ld\n",
            current->name, current->pid, status);
    current->state = PROC_ZOMBIE;
    yield();
    /* Should never reach here */
    while (1) asm volatile("wfi");
}


/*
 * syscall_dispatch — Called from the exception handler for SVC exceptions
 *
 * The trap frame pointer lets us read arguments and set the return value.
 */
void syscall_dispatch(uint64_t *frame) {
    trap_frame_t *tf = (trap_frame_t *)frame;

    uint64_t syscall_num = tf->regs[8];     /* x8 */
    uint64_t arg0 = tf->regs[0];            /* x0 */
    uint64_t arg1 = tf->regs[1];            /* x1 */
    uint64_t arg2 = tf->regs[2];            /* x2 */

    int64_t ret = 0;

    kprintf("[syscall] num=%lu from PID %lu\n", syscall_num, proc_current()->pid);

    switch (syscall_num) {
    case SYS_WRITE:
        ret = sys_write(arg0, arg1, arg2);
        break;
    case SYS_EXIT:
        sys_exit((int64_t)arg0);
        break;
    case SYS_YIELD:
        yield();
        break;
    case SYS_GETPID:
        ret = (int64_t)proc_current()->pid;
        break;
    default:
        kprintf("[syscall] Unknown syscall %lu from PID %lu\n",
                syscall_num, proc_current()->pid);
        ret = -1;
        break;
    }

    /* Set return value in x0 */
    tf->regs[0] = (uint64_t)ret;
}
