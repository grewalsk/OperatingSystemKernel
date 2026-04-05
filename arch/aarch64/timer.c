/*
 * timer.c — ARM Generic Timer driver
 *
 * AArch64 CPUs have a built-in timer accessible via system registers.
 * We use the physical timer (EL1) to generate periodic interrupts
 * that drive the scheduler (in later phases).
 *
 * Key registers:
 *   CNTFRQ_EL0  — Counter frequency (Hz)
 *   CNTP_TVAL_EL0 — Timer value (counts down, fires IRQ at 0)
 *   CNTP_CTL_EL0  — Timer control (enable, mask, status)
 *   CNTPCT_EL0  — Current counter value (read-only, always counting)
 *
 * The timer generates PPI interrupt 30 (INTID 30) on GICv3.
 */

#include "kernel.h"
#include "drivers/timer.h"
#include "drivers/gic.h"
#include "drivers/uart.h"

/* Timer control register bits */
#define CNTP_CTL_ENABLE     (1 << 0)
#define CNTP_CTL_IMASK      (1 << 1)    /* Interrupt mask */
#define CNTP_CTL_ISTATUS    (1 << 2)    /* Interrupt status */

static uint64_t timer_freq;
static volatile uint64_t tick_count = 0;


/* Read the timer frequency */
static inline uint64_t read_cntfrq(void) {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

/* Set the timer countdown value */
static inline void write_cntp_tval(uint64_t val) {
    asm volatile("msr cntp_tval_el0, %0" :: "r"(val));
}

/* Set timer control */
static inline void write_cntp_ctl(uint64_t val) {
    asm volatile("msr cntp_ctl_el0, %0" :: "r"(val));
}

/* Read current counter */
static inline uint64_t read_cntpct(void) {
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}


void timer_init(void) {
    timer_freq = read_cntfrq();

    kprintf("[timer] Frequency: %lu Hz\n", timer_freq);
    kprintf("[timer] Current counter: %lu\n", read_cntpct());

    /* Enable the timer IRQ in the GIC (PPI 30) */
    gic_enable_irq(TIMER_IRQ);

    /* Set initial countdown — 1 second */
    write_cntp_tval(timer_freq);

    /* Enable timer, unmask interrupt */
    write_cntp_ctl(CNTP_CTL_ENABLE);

    kprintf("[timer] Timer armed (1s interval)\n");
}


void timer_handle_irq(void) {
    tick_count++;
    kprintf("[timer] Tick %lu\n", tick_count);

    /* Re-arm the timer for the next tick */
    write_cntp_tval(timer_freq);
}


uint64_t timer_get_ticks(void) {
    return tick_count;
}


uint64_t timer_get_freq(void) {
    return timer_freq;
}
