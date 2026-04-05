/*
 * exception.S — AArch64 Exception Vector Table
 *
 * AArch64 has a vector table with 16 entries, organized as:
 *   4 exception types × 4 source contexts = 16 entries
 *
 * Exception types:
 *   - Synchronous (SVC, data abort, etc.)
 *   - IRQ (hardware interrupts)
 *   - FIQ (fast interrupts — unused)
 *   - SError (system errors)
 *
 * Source contexts:
 *   - Current EL with SP_EL0
 *   - Current EL with SP_ELx (this is what we use in kernel)
 *   - Lower EL using AArch64
 *   - Lower EL using AArch32
 *
 * Each entry is 128 bytes (32 instructions). The table must be
 * aligned to 2048 bytes (0x800).
 */

.section ".text"

/* Save all general-purpose registers to the stack */
.macro save_regs
    sub     sp, sp, #(34 * 8)       /* 31 GPRs + SP + ELR + SPSR = 34 */
    stp     x0,  x1,  [sp, #(0  * 8)]
    stp     x2,  x3,  [sp, #(2  * 8)]
    stp     x4,  x5,  [sp, #(4  * 8)]
    stp     x6,  x7,  [sp, #(6  * 8)]
    stp     x8,  x9,  [sp, #(8  * 8)]
    stp     x10, x11, [sp, #(10 * 8)]
    stp     x12, x13, [sp, #(12 * 8)]
    stp     x14, x15, [sp, #(14 * 8)]
    stp     x16, x17, [sp, #(16 * 8)]
    stp     x18, x19, [sp, #(18 * 8)]
    stp     x20, x21, [sp, #(20 * 8)]
    stp     x22, x23, [sp, #(22 * 8)]
    stp     x24, x25, [sp, #(24 * 8)]
    stp     x26, x27, [sp, #(26 * 8)]
    stp     x28, x29, [sp, #(28 * 8)]

    /* Save x30 (LR), ELR_EL1 (return address), SPSR_EL1 (saved state) */
    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x30, x0,  [sp, #(30 * 8)]
    str     x1,  [sp, #(32 * 8)]
.endm

/* Restore all general-purpose registers from the stack */
.macro restore_regs
    /* Restore ELR_EL1 and SPSR_EL1 first */
    ldp     x30, x0,  [sp, #(30 * 8)]
    ldr     x1,  [sp, #(32 * 8)]
    msr     elr_el1, x0
    msr     spsr_el1, x1

    ldp     x0,  x1,  [sp, #(0  * 8)]
    ldp     x2,  x3,  [sp, #(2  * 8)]
    ldp     x4,  x5,  [sp, #(4  * 8)]
    ldp     x6,  x7,  [sp, #(6  * 8)]
    ldp     x8,  x9,  [sp, #(8  * 8)]
    ldp     x10, x11, [sp, #(10 * 8)]
    ldp     x12, x13, [sp, #(12 * 8)]
    ldp     x14, x15, [sp, #(14 * 8)]
    ldp     x16, x17, [sp, #(16 * 8)]
    ldp     x18, x19, [sp, #(18 * 8)]
    ldp     x20, x21, [sp, #(20 * 8)]
    ldp     x22, x23, [sp, #(22 * 8)]
    ldp     x24, x25, [sp, #(24 * 8)]
    ldp     x26, x27, [sp, #(26 * 8)]
    ldp     x28, x29, [sp, #(28 * 8)]

    add     sp, sp, #(34 * 8)
.endm

/*
 * Vector entry macro — each entry must be exactly 128 bytes (0x80).
 * We save registers, set up the exception type argument, and call
 * the C handler.
 */
.macro vector_entry handler
    .balign 0x80
    save_regs
    mov     x0, sp              /* Pass pointer to saved registers */
    bl      \handler
    restore_regs
    eret                        /* Return from exception */
.endm

/* Invalid exception — should never happen */
.macro vector_entry_invalid type
    .balign 0x80
    save_regs
    mov     x0, #\type          /* Exception type number */
    mov     x1, sp              /* Pointer to saved registers */
    bl      exception_invalid
    restore_regs
    eret
.endm


/*
 * Exception Vector Table
 * Must be aligned to 2048 bytes (0x800)
 */
.balign 0x800
.global exception_vector_table
exception_vector_table:

    /* ─── Current EL with SP_EL0 (fallback — also handle IRQ here) ─── */
    vector_entry_invalid 0      /* Synchronous */
    vector_entry el1_irq        /* IRQ — handle even if SP_EL0 */
    vector_entry el1_irq        /* FIQ — treat like IRQ */
    vector_entry_invalid 3      /* SError */

    /* ─── Current EL with SP_ELx (kernel exceptions) ─── */
    vector_entry el1_sync       /* Synchronous */
    vector_entry el1_irq        /* IRQ */
    vector_entry el1_irq        /* FIQ — treat like IRQ */
    vector_entry_invalid 7      /* SError */

    /* ─── Lower EL using AArch64 (user space — used later) ─── */
    vector_entry_invalid 8      /* Synchronous */
    vector_entry_invalid 9      /* IRQ */
    vector_entry_invalid 10     /* FIQ */
    vector_entry_invalid 11     /* SError */

    /* ─── Lower EL using AArch32 (not supported) ─── */
    vector_entry_invalid 12     /* Synchronous */
    vector_entry_invalid 13     /* IRQ */
    vector_entry_invalid 14     /* FIQ */
    vector_entry_invalid 15     /* SError */
