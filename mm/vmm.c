/*
 * vmm.c — Virtual Memory Manager (AArch64 page tables)
 *
 * Manages the AArch64 4-level page table hierarchy:
 *   L0 (PGD) → L1 (PUD) → L2 (PMD) → L3 (PTE) → 4KB page
 *
 * Two page table bases:
 *   TTBR0_EL1 — maps lower addresses (user space: 0x0000...)
 *   TTBR1_EL1 — maps upper addresses (kernel: 0xFFFF...)
 *
 * For Phase 3, we set up an identity map (VA == PA) for the kernel
 * so the MMU can be enabled without breaking existing code.
 * The higher-half kernel mapping comes later.
 *
 * Page table entry format (4KB granule):
 *   [47:12] — Output address (physical page frame)
 *   [11:0]  — Attributes (access permissions, cacheability, etc.)
 *
 * Descriptor types (bits [1:0]):
 *   0b00 — Invalid (fault on access)
 *   0b01 — Block descriptor (L1: 1GB, L2: 2MB)
 *   0b11 — Table descriptor (L0-L2) or Page descriptor (L3)
 */

#include "kernel.h"
#include "mm.h"
#include "drivers/uart.h"

/* Page table entry bits */
#define PTE_VALID       (1UL << 0)
#define PTE_TABLE       (1UL << 1)  /* Table descriptor (for L0-L2) */
#define PTE_PAGE        (1UL << 1)  /* Page descriptor (for L3) */
#define PTE_AF          (1UL << 10) /* Access Flag (must be set) */
#define PTE_SH_INNER    (3UL << 8)  /* Inner Shareable */
#define PTE_AP_RW       (0UL << 6)  /* EL1 R/W, EL0 none */
#define PTE_AP_RO       (2UL << 6)  /* EL1 R/O, EL0 none */
#define PTE_UXN         (1UL << 54) /* Unprivileged Execute Never */
#define PTE_PXN         (1UL << 53) /* Privileged Execute Never */

/* MAIR attribute indices (set up in mmu_init) */
#define MAIR_IDX_DEVICE     0   /* Device-nGnRnE (MMIO) */
#define MAIR_IDX_NORMAL     1   /* Normal cacheable (RAM) */

#define PTE_DEVICE  ((MAIR_IDX_DEVICE << 2) | PTE_AF | PTE_SH_INNER | PTE_UXN | PTE_PXN)
#define PTE_NORMAL  ((MAIR_IDX_NORMAL << 2) | PTE_AF | PTE_SH_INNER)

/* Extract table index from virtual address */
#define L0_INDEX(va)    ((((uint64_t)(va)) >> 39) & 0x1FF)
#define L1_INDEX(va)    ((((uint64_t)(va)) >> 30) & 0x1FF)
#define L2_INDEX(va)    ((((uint64_t)(va)) >> 21) & 0x1FF)
#define L3_INDEX(va)    ((((uint64_t)(va)) >> 12) & 0x1FF)

#define ENTRIES_PER_TABLE   512


/*
 * Allocate a page table (one 4KB page, zeroed).
 * Uses the physical memory allocator.
 */
static uint64_t *alloc_page_table(void) {
    void *page = pmm_alloc_page();
    if (!page) {
        panic("vmm: failed to allocate page table");
    }
    return (uint64_t *)page;
}


/*
 * Walk the page table, creating intermediate tables as needed.
 * Returns a pointer to the L3 PTE for the given virtual address.
 */
static uint64_t *vmm_walk(uint64_t *l0_table, uint64_t va, bool create) {
    /* L0 → L1 */
    uint64_t l0_idx = L0_INDEX(va);
    uint64_t *l1_table;
    if (l0_table[l0_idx] & PTE_VALID) {
        l1_table = (uint64_t *)(l0_table[l0_idx] & 0x0000FFFFFFFFF000UL);
    } else {
        if (!create) return NULL;
        l1_table = alloc_page_table();
        l0_table[l0_idx] = (uint64_t)l1_table | PTE_VALID | PTE_TABLE;
    }

    /* L1 → L2 */
    uint64_t l1_idx = L1_INDEX(va);
    uint64_t *l2_table;
    if (l1_table[l1_idx] & PTE_VALID) {
        l2_table = (uint64_t *)(l1_table[l1_idx] & 0x0000FFFFFFFFF000UL);
    } else {
        if (!create) return NULL;
        l2_table = alloc_page_table();
        l1_table[l1_idx] = (uint64_t)l2_table | PTE_VALID | PTE_TABLE;
    }

    /* L2 → L3 */
    uint64_t l2_idx = L2_INDEX(va);
    uint64_t *l3_table;
    if (l2_table[l2_idx] & PTE_VALID) {
        l3_table = (uint64_t *)(l2_table[l2_idx] & 0x0000FFFFFFFFF000UL);
    } else {
        if (!create) return NULL;
        l3_table = alloc_page_table();
        l2_table[l2_idx] = (uint64_t)l3_table | PTE_VALID | PTE_TABLE;
    }

    /* Return pointer to L3 entry */
    return &l3_table[L3_INDEX(va)];
}


void vmm_map_page(uint64_t *l0_table, uint64_t va, uint64_t pa, uint64_t flags) {
    uint64_t *pte = vmm_walk(l0_table, va, true);
    *pte = (pa & 0x0000FFFFFFFFF000UL) | flags | PTE_VALID | PTE_PAGE;
}


void vmm_unmap_page(uint64_t *l0_table, uint64_t va) {
    uint64_t *pte = vmm_walk(l0_table, va, false);
    if (pte) {
        *pte = 0;
    }
}


uint64_t vmm_virt_to_phys(uint64_t *l0_table, uint64_t va) {
    uint64_t *pte = vmm_walk(l0_table, va, false);
    if (!pte || !(*pte & PTE_VALID)) {
        return 0;  /* Not mapped */
    }
    return (*pte & 0x0000FFFFFFFFF000UL) | (va & 0xFFF);
}


/*
 * Create an identity-mapped page table for the kernel.
 * Maps:
 *   - Device memory (0x00000000 - 0x40000000): UART, GIC, etc.
 *   - RAM (0x40000000 - 0x80000000): kernel + free memory
 *
 * Uses 2MB block mappings at L2 level for efficiency
 * (no L3 tables needed for the identity map).
 */
uint64_t *vmm_create_kernel_tables(void) {
    uint64_t *l0 = alloc_page_table();

    kprintf("[vmm] Creating identity-mapped kernel page tables...\n");

    /*
     * Both 0x00000000 (device) and 0x40000000 (RAM) fall under L0 index 0
     * (both are below 512GB), so they share the same L1 table.
     */
    uint64_t *l1 = alloc_page_table();
    l0[0] = (uint64_t)l1 | PTE_VALID | PTE_TABLE;

    /*
     * L1[0] = 1GB block at 0x00000000 (device memory)
     * Block descriptor: bits[1:0] = 0b01 (PTE_VALID only, no PTE_TABLE)
     */
    l1[L1_INDEX(0x00000000UL)] = 0x00000000UL | PTE_VALID |
                                  PTE_DEVICE | PTE_AP_RW;

    /*
     * L1[1] = 1GB RAM at 0x40000000
     * Use L2 2MB block descriptors for finer granularity
     */
    uint64_t *l2_ram = alloc_page_table();
    l1[L1_INDEX(0x40000000UL)] = (uint64_t)l2_ram | PTE_VALID | PTE_TABLE;

    /* Map 512 × 2MB blocks = 1GB of RAM */
    for (uint64_t i = 0; i < 512; i++) {
        uint64_t pa = 0x40000000UL + (i * 0x200000UL);
        /* L2 block descriptor: bits[1:0] = 0b01 */
        l2_ram[i] = pa | PTE_VALID |
                    PTE_NORMAL | PTE_AP_RW | PTE_SH_INNER | PTE_AF;
    }

    kprintf("[vmm] Identity map: device 0x0-0x40000000, RAM 0x40000000-0x80000000\n");

    return l0;
}


/*
 * Enable the MMU.
 *
 * Steps:
 *   1. Set MAIR_EL1 (memory attribute indirection register)
 *   2. Set TCR_EL1 (translation control register)
 *   3. Set TTBR0_EL1 (page table base)
 *   4. Enable MMU in SCTLR_EL1
 */
void vmm_enable_mmu(uint64_t *l0_table) {
    kprintf("[vmm] Enabling MMU...\n");

    /* MAIR: Define memory attribute types */
    uint64_t mair = (0x00UL << (MAIR_IDX_DEVICE * 8)) |  /* Device-nGnRnE */
                    (0xFFUL << (MAIR_IDX_NORMAL * 8));    /* Normal, Write-Back, Cacheable */
    asm volatile("msr mair_el1, %0" :: "r"(mair));

    /*
     * TCR_EL1: Translation Control Register
     *   T0SZ = 16:  48-bit VA space for TTBR0 (256TB)
     *   TG0 = 0b00: 4KB granule for TTBR0
     *   SH0 = 0b11: Inner Shareable
     *   ORGN0 = 0b01: Write-Back, Write-Allocate cacheable
     *   IRGN0 = 0b01: Write-Back, Write-Allocate cacheable
     *   IPS = 0b010: 40-bit physical address space (1TB)
     */
    uint64_t tcr = (16UL << 0)   |  /* T0SZ = 16 */
                   (0b00UL << 14) |  /* TG0 = 4KB */
                   (0b11UL << 12) |  /* SH0 = Inner Shareable */
                   (0b01UL << 10) |  /* ORGN0 = Write-Back WA */
                   (0b01UL << 8)  |  /* IRGN0 = Write-Back WA */
                   (0b010UL << 32);  /* IPS = 40-bit */
    asm volatile("msr tcr_el1, %0" :: "r"(tcr));

    /* Set TTBR0 to our page table */
    asm volatile("msr ttbr0_el1, %0" :: "r"((uint64_t)l0_table));

    /* Ensure all writes are visible before enabling MMU */
    asm volatile("isb");
    asm volatile("dsb sy");

    /* Enable MMU via SCTLR_EL1 */
    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0);   /* M = 1: Enable MMU */
    sctlr |= (1 << 2);   /* C = 1: Enable data cache */
    sctlr |= (1 << 12);  /* I = 1: Enable instruction cache */
    asm volatile("msr sctlr_el1, %0" :: "r"(sctlr));
    asm volatile("isb");

    kprintf("[vmm] MMU enabled! Running with virtual memory.\n");
}
