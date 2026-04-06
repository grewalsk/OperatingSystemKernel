/*
 * context.S — AArch64 context switch
 *
 * Saves callee-saved registers of the old process and restores
 * those of the new process. This is the core of multitasking.
 *
 * void context_switch(cpu_context_t *old, cpu_context_t *new);
 *   x0 = pointer to old context (save current registers here)
 *   x1 = pointer to new context (load registers from here)
 *
 * We only save callee-saved registers (x19-x30, sp) because:
 *   - Caller-saved registers (x0-x18) are already saved by the
 *     C calling convention before calling context_switch()
 *   - The compiler generates code that doesn't expect these to
 *     survive across function calls
 */

.global context_switch
context_switch:
    /* Save old context */
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x30, [x0, #80]    /* fp, lr */
    mov     x2, sp
    str     x2, [x0, #96]          /* sp */

    /* Load new context */
    ldp     x19, x20, [x1, #0]
    ldp     x21, x22, [x1, #16]
    ldp     x23, x24, [x1, #32]
    ldp     x25, x26, [x1, #48]
    ldp     x27, x28, [x1, #64]
    ldp     x29, x30, [x1, #80]    /* fp, lr */
    ldr     x2, [x1, #96]
    mov     sp, x2                  /* sp */

    /* Return to the new process.
     * 'ret' branches to x30 (lr), which is the return address
     * of the new process — either where it was when it was
     * switched away from, or the entry point for a new process. */
    ret
