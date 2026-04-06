/*
 * kmalloc.c — Kernel heap allocator
 *
 * Simple free-list allocator for variable-sized kernel allocations.
 * Uses a linked list of free blocks with first-fit allocation.
 *
 * The heap grows by requesting pages from the physical allocator.
 * Each allocation has a small header storing the size.
 */

#include "kernel.h"
#include "mm.h"
#include "drivers/uart.h"

/* Allocation header — sits before each allocated block */
typedef struct alloc_header {
    uint64_t size;          /* Size of the block (including header) */
    uint64_t magic;         /* Magic number for corruption detection */
} alloc_header_t;

/* Free block in the free list */
typedef struct free_block {
    uint64_t size;          /* Total size of this free block */
    struct free_block *next;
} free_block_t;

#define ALLOC_MAGIC     0xA110CA7EDEADBEEFULL
#define MIN_BLOCK_SIZE  (sizeof(free_block_t))
#define HEAP_PAGE_COUNT 16  /* Grow heap by 64KB at a time */

static free_block_t *free_list = NULL;
static uint64_t heap_total = 0;
static uint64_t heap_used = 0;


/* Grow the heap by allocating more pages */
static bool heap_grow(void) {
    /* Allocate contiguous pages */
    void *first_page = pmm_alloc_page();
    if (!first_page) return false;

    uint64_t block_size = PAGE_SIZE;

    /* Try to get more contiguous pages */
    for (int i = 1; i < HEAP_PAGE_COUNT; i++) {
        void *page = pmm_alloc_page();
        if (!page) break;
        /* Check if contiguous */
        if ((uint64_t)page == (uint64_t)first_page + block_size) {
            block_size += PAGE_SIZE;
        } else {
            /* Not contiguous — free it back and stop */
            pmm_free_page(page);
            break;
        }
    }

    /* Add the new memory as a free block */
    free_block_t *block = (free_block_t *)first_page;
    block->size = block_size;
    block->next = free_list;
    free_list = block;

    heap_total += block_size;
    return true;
}


void kmalloc_init(void) {
    free_list = NULL;
    heap_total = 0;
    heap_used = 0;

    /* Pre-allocate initial heap */
    heap_grow();

    kprintf("[kmalloc] Heap initialized: %lu KB\n", heap_total / 1024);
}


void *kmalloc(uint64_t size) {
    if (size == 0) return NULL;

    /* Align size to 16 bytes and add header */
    uint64_t total = sizeof(alloc_header_t) + size;
    total = (total + 15) & ~15UL;

    /* First-fit search */
    free_block_t **prev = &free_list;
    free_block_t *curr = free_list;

    while (curr) {
        if (curr->size >= total) {
            /* Found a fit */
            uint64_t remaining = curr->size - total;

            if (remaining >= MIN_BLOCK_SIZE + 16) {
                /* Split the block */
                free_block_t *new_free = (free_block_t *)((uint8_t *)curr + total);
                new_free->size = remaining;
                new_free->next = curr->next;
                *prev = new_free;
            } else {
                /* Use the entire block */
                total = curr->size;
                *prev = curr->next;
            }

            /* Set up allocation header */
            alloc_header_t *header = (alloc_header_t *)curr;
            header->size = total;
            header->magic = ALLOC_MAGIC;

            heap_used += total;
            return (void *)((uint8_t *)header + sizeof(alloc_header_t));
        }

        prev = &curr->next;
        curr = curr->next;
    }

    /* No block big enough — grow the heap and retry */
    if (heap_grow()) {
        return kmalloc(size);  /* Retry after growing */
    }

    kprintf("[kmalloc] ERROR: Out of memory (requested %lu bytes)\n", size);
    return NULL;
}


void kfree(void *ptr) {
    if (!ptr) return;

    alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));

    /* Check for corruption */
    if (header->magic != ALLOC_MAGIC) {
        panic("kfree: heap corruption detected!");
    }

    uint64_t size = header->size;
    header->magic = 0;  /* Invalidate */

    /* Add to free list (simple prepend — no coalescing yet) */
    free_block_t *block = (free_block_t *)header;
    block->size = size;
    block->next = free_list;
    free_list = block;

    heap_used -= size;
}


uint64_t kmalloc_used(void) {
    return heap_used;
}
