#ifndef DRIVERS_TIMER_H
#define DRIVERS_TIMER_H

#include "kernel.h"

/*
 * ARM Generic Timer Driver
 *
 * Timer interrupt is PPI 30 (INTID 30) on GICv3.
 */

#define TIMER_IRQ   30

/* Initialize and start the periodic timer */
void timer_init(void);

/* Handle a timer IRQ (called from exception.c) */
void timer_handle_irq(void);

/* Get the number of timer ticks since boot */
uint64_t timer_get_ticks(void);

/* Get the timer frequency in Hz */
uint64_t timer_get_freq(void);

/* Check and clear the reschedule flag */
bool timer_needs_reschedule(void);

#endif /* DRIVERS_TIMER_H */
