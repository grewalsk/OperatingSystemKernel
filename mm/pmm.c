/*
 * pmm.c — Physical Memory Manager (bitmap-based page allocator)
 *
 * Manages physical RAM as 4KB pages using a simple bitmap.
 * Each bit represents one page: 1 = allocated, 0 = free.
 *
 * On QEMU virt with 1GB RAM:
 *   RAM starts at 0x40000000 (1GB mark)
 *   RAM ends at   0x80000000 (2GB mark)
 *   Total pages:  262144 (1GB / 4KB)
 *   Bitmap size:  32768 bytes (262144 / 8)
 *
 * The bitmap itself lives right after the kernel image.
 * Pages occupied by the kernel + bitmap are marked as allocated.
 */

#include "kernel.h"
#include "mm.h"
#include "drivers/uart.h"

/* Physical memory boundaries */
#define RAM_START       0x40000000UL
#define RAM_END         0x80000000UL
#define RAM_SIZE        (RAM_END - RAM_START)

/* Page size: 4KB */
#define PAGE_SIZE       4096
#define TOTAL_PAGES     (RAM_SIZE / PAGE_SIZE)

/* Bitmap: 1 bit per page */
static uint8_t page_bitmap[TOTAL_PAGES / 8];

/* Statistics */
static uint64_t total_pages;
static uint64_t used_pages;

/* Linker symbol: end of kernel image */
extern uint8_t __kernel_end;


/* Set a page as allocated in the bitmap */
static inline void bitmap_set(uint64_t page_idx) {
    page_bitmap[page_idx / 8] |= (1 << (page_idx % 8));
}

/* Set a page as free in the bitmap */
static inline void bitmap_clear(uint64_t page_idx) {
    page_bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
}

/* Check if a page is allocated */
static inline bool bitmap_test(uint64_t page_idx) {
    return (page_bitmap[page_idx / 8] >> (page_idx % 8)) & 1;
}

/* Convert physical address to page index */
static inline uint64_t addr_to_page(uint64_t addr) {
    return (addr - RAM_START) / PAGE_SIZE;
}

/* Convert page index to physical address */
static inline uint64_t page_to_addr(uint64_t page_idx) {
    return RAM_START + (page_idx * PAGE_SIZE);
}


void pmm_init(void) {
    total_pages = TOTAL_PAGES;
    used_pages = 0;

    /* Clear bitmap — all pages start as free */
    for (uint64_t i = 0; i < sizeof(page_bitmap); i++) {
        page_bitmap[i] = 0;
    }

    /*
     * Mark kernel pages as allocated.
     * Everything from RAM_START to __kernel_end is used by the kernel
     * (includes .text, .data, .bss, and the stack).
     */
    uint64_t kernel_end = (uint64_t)&__kernel_end;
    /* Align up to page boundary */
    kernel_end = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uint64_t kernel_pages = (kernel_end - RAM_START) / PAGE_SIZE;

    for (uint64_t i = 0; i < kernel_pages; i++) {
        bitmap_set(i);
        used_pages++;
    }

    kprintf("[pmm] Physical memory: %lu MB (%lu pages)\n",
            RAM_SIZE / (1024 * 1024), total_pages);
    kprintf("[pmm] Kernel uses: %lu pages (%lu KB)\n",
            kernel_pages, kernel_pages * 4);
    kprintf("[pmm] Free pages: %lu (%lu MB)\n",
            total_pages - used_pages,
            (total_pages - used_pages) * 4 / 1024);
}


void *pmm_alloc_page(void) {
    /* Linear scan for a free page */
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;

            /* Zero the page before returning */
            uint64_t addr = page_to_addr(i);
            uint64_t *ptr = (uint64_t *)addr;
            for (uint64_t j = 0; j < PAGE_SIZE / sizeof(uint64_t); j++) {
                ptr[j] = 0;
            }

            return (void *)addr;
        }
    }

    /* Out of memory */
    kprintf("[pmm] ERROR: Out of physical memory!\n");
    return NULL;
}


void pmm_free_page(void *addr) {
    uint64_t page_addr = (uint64_t)addr;

    /* Sanity checks */
    if (page_addr < RAM_START || page_addr >= RAM_END) {
        kprintf("[pmm] ERROR: Free address 0x%lx out of range!\n", page_addr);
        return;
    }
    if (page_addr & (PAGE_SIZE - 1)) {
        kprintf("[pmm] ERROR: Free address 0x%lx not page-aligned!\n", page_addr);
        return;
    }

    uint64_t idx = addr_to_page(page_addr);
    if (!bitmap_test(idx)) {
        kprintf("[pmm] WARNING: Double-free of page 0x%lx!\n", page_addr);
        return;
    }

    bitmap_clear(idx);
    used_pages--;
}


uint64_t pmm_free_page_count(void) {
    return total_pages - used_pages;
}


uint64_t pmm_used_page_count(void) {
    return used_pages;
}
