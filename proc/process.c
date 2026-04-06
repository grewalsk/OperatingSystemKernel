/*
 * process.c — Process creation and management
 *
 * Each process has a kernel stack and a saved CPU context.
 * For now (Phase 4), all processes are kernel threads — they
 * run in EL1 (kernel mode) and share the kernel address space.
 * User-space processes come in Phase 5.
 */

#include "kernel.h"
#include "proc.h"
#include "mm.h"
#include "drivers/uart.h"

/* Process table — fixed array for simplicity */
static process_t proc_table[MAX_PROCS];
static uint64_t next_pid = 1;
static process_t *current_proc = NULL;

/*
 * Trampoline for new kernel threads.
 * When a new process is context-switched to for the first time,
 * it "returns" to this function (via lr in the saved context).
 * x19 holds the actual entry point.
 */
static void kthread_trampoline(void) {
    /* The entry point was stashed in x19 by proc_create_kthread */
    void (*entry)(void);
    asm volatile("mov %0, x19" : "=r"(entry));
    entry();

    /* If the thread returns, mark it as zombie */
    kprintf("[proc] Thread '%s' (PID %lu) exited\n",
            current_proc->name, current_proc->pid);
    current_proc->state = PROC_ZOMBIE;

    /* Yield to another process */
    while (1) {
        schedule();
    }
}


void proc_init(void) {
    /* Clear all process table entries */
    for (int i = 0; i < MAX_PROCS; i++) {
        proc_table[i].state = PROC_UNUSED;
        proc_table[i].pid = 0;
    }

    /*
     * Create process 0 — the "idle" / boot process.
     * This represents the current execution context.
     * Its context will be saved when we first context_switch away.
     */
    proc_table[0].pid = 0;
    proc_table[0].state = PROC_RUNNING;
    proc_table[0].name = "idle";
    proc_table[0].kstack = NULL;  /* Uses the boot stack */
    current_proc = &proc_table[0];

    kprintf("[proc] Process subsystem initialized (max %d processes)\n", MAX_PROCS);
}


process_t *proc_create_kthread(const char *name, void (*entry)(void)) {
    /* Find a free slot */
    process_t *p = NULL;
    for (int i = 1; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            p = &proc_table[i];
            break;
        }
    }
    if (!p) {
        kprintf("[proc] ERROR: No free process slots!\n");
        return NULL;
    }

    /* Allocate kernel stack — use a single page (4KB) for now.
     * This is simpler and avoids contiguity issues with the bitmap allocator. */
    void *stack_base = pmm_alloc_page();
    if (!stack_base) {
        panic("proc_create_kthread: out of memory for stack");
    }

    /* Stack top (stack grows downward) */
    uint64_t stack_top = (uint64_t)stack_base + PAGE_SIZE;

    /* Set up the process */
    p->pid = next_pid++;
    p->state = PROC_CREATED;
    p->name = name;
    p->kstack = stack_base;
    p->next = NULL;

    /*
     * Set up initial context so that when context_switch loads it:
     *   - lr (x30) points to kthread_trampoline
     *   - sp points to the top of the kernel stack
     *   - x19 holds the actual entry function pointer
     *     (kthread_trampoline reads x19 to find where to jump)
     */
    p->context.lr = (uint64_t)kthread_trampoline;
    p->context.sp = stack_top;
    p->context.fp = 0;
    p->context.x19 = (uint64_t)entry;
    /* Other registers initialized to 0 (already zeroed by pmm_alloc_page) */

    kprintf("[proc] Created kthread '%s' (PID %lu)\n", name, p->pid);
    return p;
}


process_t *proc_current(void) {
    return current_proc;
}


void proc_set_current(process_t *p) {
    current_proc = p;
}
