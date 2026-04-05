/*
 * gic.c — GICv2 Interrupt Controller Driver
 *
 * Using GICv2 for simplicity — purely MMIO-based, no system register
 * access needed (avoids EL2/EL3 trapping issues).
 *
 * GICv2 architecture:
 *   - Distributor (GICD): Routes interrupts to CPU interfaces
 *   - CPU Interface (GICC): Per-CPU interrupt handling
 *
 * On QEMU virt with gic-version=2:
 *   GICD base: 0x08000000
 *   GICC base: 0x08010000
 */

#include "kernel.h"
#include "drivers/gic.h"
#include "drivers/uart.h"

/* GICD (Distributor) registers */
#define GICD_BASE           0x08000000UL
#define GICD_CTLR           (*(volatile uint32_t *)(GICD_BASE + 0x000))
#define GICD_TYPER          (*(volatile uint32_t *)(GICD_BASE + 0x004))
#define GICD_ISENABLER(n)   (*(volatile uint32_t *)(GICD_BASE + 0x100 + (n) * 4))
#define GICD_ICENABLER(n)   (*(volatile uint32_t *)(GICD_BASE + 0x180 + (n) * 4))
#define GICD_IPRIORITYR(n)  (*(volatile uint8_t  *)(GICD_BASE + 0x400 + (n)))
#define GICD_ITARGETSR(n)   (*(volatile uint8_t  *)(GICD_BASE + 0x800 + (n)))
#define GICD_ICFGR(n)       (*(volatile uint32_t *)(GICD_BASE + 0xC00 + (n) * 4))
#define GICD_IGROUPR(n)     (*(volatile uint32_t *)(GICD_BASE + 0x080 + (n) * 4))

/* GICC (CPU Interface) registers */
#define GICC_BASE           0x08010000UL
#define GICC_CTLR           (*(volatile uint32_t *)(GICC_BASE + 0x000))
#define GICC_PMR            (*(volatile uint32_t *)(GICC_BASE + 0x004))
#define GICC_IAR            (*(volatile uint32_t *)(GICC_BASE + 0x00C))
#define GICC_EOIR           (*(volatile uint32_t *)(GICC_BASE + 0x010))

/* GICD_CTLR bits */
#define GICD_CTLR_EN_GRP0   (1 << 0)
#define GICD_CTLR_EN_GRP1   (1 << 1)

/* GICC_CTLR bits */
#define GICC_CTLR_EN_GRP0   (1 << 0)    /* Enable Group 0 (signaled as FIQ) */
#define GICC_CTLR_EN_GRP1   (1 << 1)    /* Enable Group 1 (signaled as IRQ) */
#define GICC_CTLR_FIQEN     (1 << 3)    /* FIQ enable for Group 0 */


void gic_init(void) {
    kprintf("[gic] Initializing GICv2...\n");

    /* Disable distributor and CPU interface while configuring */
    GICD_CTLR = 0;
    GICC_CTLR = 0;

    /* Read how many IRQ lines are supported */
    uint32_t typer = GICD_TYPER;
    uint32_t num_irqs = ((typer & 0x1F) + 1) * 32;
    kprintf("[gic] Supports %u IRQs\n", num_irqs);

    /* Disable all interrupts */
    for (uint32_t i = 0; i < num_irqs / 32; i++) {
        GICD_ICENABLER(i) = 0xFFFFFFFF;
    }

    /* Put ALL interrupts into Group 0 */
    for (uint32_t i = 0; i < num_irqs / 32; i++) {
        GICD_IGROUPR(i) = 0x00000000;
    }

    /* Set all interrupt priorities to medium (0xA0) */
    for (uint32_t i = 0; i < num_irqs; i++) {
        GICD_IPRIORITYR(i) = 0xA0;
    }

    /* Route all SPIs to CPU 0 */
    for (uint32_t i = 32; i < num_irqs; i++) {
        GICD_ITARGETSR(i) = 0x01;  /* CPU 0 */
    }

    /* Set priority mask — accept all priorities */
    GICC_PMR = 0xFF;

    /* Enable distributor for Group 0 */
    GICD_CTLR = GICD_CTLR_EN_GRP0;

    /* Enable CPU interface for Group 0, WITHOUT FIQEn → signals as IRQ */
    GICC_CTLR = GICC_CTLR_EN_GRP0;

    kprintf("[gic] GICv2 initialized\n");
}


void gic_enable_irq(uint32_t irq) {
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;

    /* Set priority */
    GICD_IPRIORITYR(irq) = 0x80;

    /* Ensure interrupt is in Group 0 */
    GICD_IGROUPR(reg) &= ~(1 << bit);

    /* For SPIs, route to CPU 0 */
    if (irq >= 32) {
        GICD_ITARGETSR(irq) = 0x01;
    }

    /* Enable the interrupt */
    GICD_ISENABLER(reg) = (1 << bit);
}


void gic_disable_irq(uint32_t irq) {
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    GICD_ICENABLER(reg) = (1 << bit);
}


uint32_t gic_acknowledge_irq(void) {
    return GICC_IAR & 0x3FF;    /* Interrupt ID in bits [9:0] */
}


void gic_end_irq(uint32_t irq) {
    GICC_EOIR = irq;
}
