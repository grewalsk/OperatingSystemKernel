/*
 * linker.ld — Linker script for ArmOS kernel
 *
 * QEMU virt machine loads the kernel image at 0x40000000 (start of RAM).
 * This script defines the memory layout of the kernel image:
 *   .text.boot  — Boot code (must be first — CPU starts executing here)
 *   .text       — Kernel code
 *   .rodata     — Read-only data (strings, constants)
 *   .data       — Initialized global variables
 *   .bss        — Uninitialized globals (zeroed by boot.S)
 *   stack       — 64KB kernel stack
 */

ENTRY(_start)

SECTIONS
{
    /* Kernel loads at start of RAM on QEMU virt */
    . = 0x40000000;

    /* Boot code MUST come first — the CPU starts executing at the load address */
    .text.boot : {
        KEEP(*(.text.boot))
    }

    /* Align to 4KB page boundary for clean mapping later */
    . = ALIGN(4096);

    .text : {
        *(.text .text.*)
    }

    . = ALIGN(4096);

    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4096);

    .data : {
        *(.data .data.*)
    }

    . = ALIGN(4096);

    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }
    __bss_end = .;

    . = ALIGN(4096);

    /* Kernel stack — 64KB, grows downward */
    . = . + 0x10000;
    _stack_top = .;

    /* Track end of kernel image for memory allocator later */
    __kernel_end = .;
}
