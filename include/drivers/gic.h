#ifndef DRIVERS_GIC_H
#define DRIVERS_GIC_H

#include "kernel.h"

/*
 * GICv3 Interrupt Controller Driver
 */

/* Initialize the GIC distributor, redistributor, and CPU interface */
void gic_init(void);

/* Enable a specific interrupt number */
void gic_enable_irq(uint32_t irq);

/* Disable a specific interrupt number */
void gic_disable_irq(uint32_t irq);

/* Acknowledge an IRQ (read IAR — returns the interrupt ID) */
uint32_t gic_acknowledge_irq(void);

/* Signal end of interrupt (write EOIR) */
void gic_end_irq(uint32_t irq);

#endif /* DRIVERS_GIC_H */
