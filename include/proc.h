#ifndef PROC_H
#define PROC_H

#include "kernel.h"

/* Maximum number of processes */
#define MAX_PROCS   64

/* Process states */
#define PROC_UNUSED     0
#define PROC_CREATED    1
#define PROC_READY      2
#define PROC_RUNNING    3
#define PROC_BLOCKED    4
#define PROC_ZOMBIE     5

/* Kernel stack size per process: 16KB */
#define KSTACK_SIZE     (4 * 4096)

/*
 * CPU context saved on context switch.
 * Only callee-saved registers need to be saved (x19-x30, sp).
 * The caller-saved registers are already saved by the calling convention.
 */
typedef struct cpu_context {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t fp;    /* x29 — frame pointer */
    uint64_t lr;    /* x30 — link register (return address) */
    uint64_t sp;    /* Stack pointer */
} cpu_context_t;

/*
 * Process Control Block (PCB)
 */
typedef struct process {
    uint64_t        pid;
    uint64_t        state;
    cpu_context_t   context;        /* Saved CPU state for context switch */
    void            *kstack;        /* Base of kernel stack allocation */
    struct process  *next;          /* Next in run queue */
    const char      *name;          /* Process name (for debugging) */
} process_t;

/* ─── Process Management ─── */

/* Initialize the process subsystem */
void proc_init(void);

/* Create a new kernel task that runs the given function */
process_t *proc_create_kthread(const char *name, void (*entry)(void));

/* Get the currently running process */
process_t *proc_current(void);

/* ─── Scheduler ─── */

/* Initialize the scheduler */
void sched_init(void);

/* Add a process to the run queue */
void sched_enqueue(process_t *proc);

/* Called on timer tick — may switch to a different process */
void schedule(void);

/* Yield the CPU — voluntarily give up the timeslice */
void yield(void);

/* Perform the actual context switch (defined in context.S) */
extern void context_switch(cpu_context_t *old, cpu_context_t *new);

#endif /* PROC_H */
