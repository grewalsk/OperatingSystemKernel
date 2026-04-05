/*
 * boot.S — AArch64 kernel entry point
 *
 * This is the first code that runs when QEMU loads the kernel image.
 * It runs at EL1 (kernel mode) on the QEMU virt machine.
 *
 * Responsibilities:
 *   1. Park all secondary CPU cores (only core 0 proceeds)
 *   2. Set up the stack pointer for core 0
 *   3. Zero the .bss section (uninitialized globals)
 *   4. Branch to kernel_main() in C
 */

.section ".text.boot"
.global _start

_start:
    /* -------------------------------------------------------
     * Step 1: Check CPU ID — only core 0 continues.
     * MPIDR_EL1 holds the core ID in bits [7:0].
     * All other cores spin forever in a WFE loop.
     * ------------------------------------------------------- */
    mrs     x0, mpidr_el1       // Read Multiprocessor Affinity Register
    and     x0, x0, #0xFF       // Extract CPU ID (Aff0 field)
    cbz     x0, primary_cpu     // If CPU 0, continue boot

    /* Secondary cores: park in a low-power wait loop */
secondary_hang:
    wfe                         // Wait For Event (low-power idle)
    b       secondary_hang      // Loop forever

primary_cpu:
    /* -------------------------------------------------------
     * Step 2: Set up the stack pointer.
     * Stack grows downward. We place it at the _start symbol
     * (0x40000000) so it grows into lower addresses, away
     * from the kernel image which sits above _start.
     *
     * Actually, we use a dedicated stack area. The linker
     * script reserves space for us.
     * ------------------------------------------------------- */
    ldr     x0, =_stack_top     // Load stack top address from linker script
    mov     sp, x0              // Set stack pointer

    /* -------------------------------------------------------
     * Step 3: Zero the .bss section.
     * C expects uninitialized globals to be zero. The linker
     * script defines __bss_start and __bss_end for us.
     * ------------------------------------------------------- */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end

bss_zero_loop:
    cmp     x0, x1              // Are we past the end?
    b.ge    bss_done            // If so, done
    str     xzr, [x0], #8      // Store zero, advance pointer by 8 bytes
    b       bss_zero_loop       // Repeat

bss_done:
    /* -------------------------------------------------------
     * Step 4: Jump to C code.
     * kernel_main() should never return. If it does, we hang.
     * ------------------------------------------------------- */
    bl      kernel_main         // Branch with link to kernel_main()

    /* If kernel_main returns (it shouldn't), hang */
hang:
    wfe
    b       hang
