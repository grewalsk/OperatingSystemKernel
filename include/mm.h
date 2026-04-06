#ifndef MM_H
#define MM_H

#include "kernel.h"

#define PAGE_SIZE   4096

/* ─── Physical Memory Manager ─── */

/* Initialize the physical page allocator */
void pmm_init(void);

/* Allocate a single 4KB page (returns physical address, zeroed) */
void *pmm_alloc_page(void);

/* Free a previously allocated page */
void pmm_free_page(void *addr);

/* Get count of free/used pages */
uint64_t pmm_free_page_count(void);
uint64_t pmm_used_page_count(void);


/* ─── Virtual Memory Manager ─── */

/* Map a virtual page to a physical page in the given page table */
void vmm_map_page(uint64_t *l0_table, uint64_t va, uint64_t pa, uint64_t flags);

/* Unmap a virtual page */
void vmm_unmap_page(uint64_t *l0_table, uint64_t va);

/* Translate virtual address to physical address */
uint64_t vmm_virt_to_phys(uint64_t *l0_table, uint64_t va);

/* Create identity-mapped kernel page tables */
uint64_t *vmm_create_kernel_tables(void);

/* Enable the MMU with the given L0 page table */
void vmm_enable_mmu(uint64_t *l0_table);


/* ─── Kernel Heap Allocator ─── */

/* Initialize the kernel heap */
void kmalloc_init(void);

/* Allocate size bytes from the kernel heap */
void *kmalloc(uint64_t size);

/* Free a kernel heap allocation */
void kfree(void *ptr);

/* Get heap usage statistics */
uint64_t kmalloc_used(void);

#endif /* MM_H */
