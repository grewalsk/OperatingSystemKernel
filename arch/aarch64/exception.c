/*
 * exception.c — Exception and IRQ dispatch handlers
 *
 * Called from exception.S after registers are saved to the stack.
 * The saved register frame is passed as a pointer in x0.
 */

#include "kernel.h"
#include "drivers/uart.h"
#include "drivers/gic.h"
#include "drivers/timer.h"

/* Exception Syndrome Register (ESR_EL1) exception class values */
#define ESR_EC_SVC64    0x15    /* SVC from AArch64 */
#define ESR_EC_DABT_EL1 0x25    /* Data abort from current EL */
#define ESR_EC_IABT_EL1 0x21    /* Instruction abort from current EL */

/* Read ESR_EL1 */
static inline uint64_t read_esr_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, esr_el1" : "=r"(val));
    return val;
}

/* Read FAR_EL1 (Fault Address Register) */
static inline uint64_t read_far_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, far_el1" : "=r"(val));
    return val;
}

/* Read ELR_EL1 (Exception Link Register — return address) */
static inline uint64_t read_elr_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, elr_el1" : "=r"(val));
    return val;
}


/*
 * el1_sync — Synchronous exception handler for kernel mode (EL1)
 *
 * Synchronous exceptions include:
 *   - SVC (system calls — used later)
 *   - Data aborts (bad memory access)
 *   - Instruction aborts (bad instruction fetch)
 */
void el1_sync(uint64_t *regs) {
    uint64_t esr = read_esr_el1();
    uint64_t ec = (esr >> 26) & 0x3F;   /* Exception Class */
    uint64_t elr = read_elr_el1();
    uint64_t far = read_far_el1();

    UNUSED(regs);

    kprintf("\n[EXCEPTION] Synchronous exception at EL1\n");
    kprintf("  ESR_EL1: 0x%016lx (EC=0x%02lx)\n", esr, ec);
    kprintf("  ELR_EL1: 0x%016lx\n", elr);
    kprintf("  FAR_EL1: 0x%016lx\n", far);

    switch (ec) {
    case ESR_EC_DABT_EL1:
        panic("Data abort in kernel at address 0x%016lx");
        break;
    case ESR_EC_IABT_EL1:
        panic("Instruction abort in kernel at address 0x%016lx");
        break;
    default:
        panic("Unhandled synchronous exception (EC=0x%02lx)");
        break;
    }
}


/*
 * el1_irq — IRQ handler for kernel mode (EL1)
 *
 * Called when a hardware interrupt fires while in the kernel.
 * Reads the GIC to identify which interrupt fired, then dispatches
 * to the appropriate handler.
 */
void el1_irq(uint64_t *regs) {
    UNUSED(regs);

    uint32_t irq = gic_acknowledge_irq();

    /* Spurious interrupts (1022, 1023) — ignore, do NOT send EOIR */
    if (irq >= 1020) {
        return;
    }

    switch (irq) {
    case TIMER_IRQ:
        timer_handle_irq();
        break;
    default:
        kprintf("[IRQ] Unknown IRQ: %u\n", irq);
        break;
    }

    gic_end_irq(irq);
}


/*
 * exception_invalid — Handler for unexpected/invalid exceptions
 *
 * If we get here, something went very wrong.
 */
void exception_invalid(uint64_t type, uint64_t *regs) {
    UNUSED(regs);

    kprintf("\n[EXCEPTION] Invalid exception (type %lu)\n", type);
    kprintf("  ESR_EL1: 0x%016lx\n", read_esr_el1());
    kprintf("  ELR_EL1: 0x%016lx\n", read_elr_el1());
    kprintf("  FAR_EL1: 0x%016lx\n", read_far_el1());

    panic("Invalid exception — system halted");
}
