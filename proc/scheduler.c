/*
 * scheduler.c — Round-robin preemptive scheduler
 *
 * Uses a simple linked-list run queue. On each timer tick,
 * the current process is moved to the back of the queue and
 * the next process runs.
 */

#include "kernel.h"
#include "proc.h"
#include "drivers/uart.h"

/* Run queue: linked list of READY processes */
static process_t *run_queue_head = NULL;
static process_t *run_queue_tail = NULL;


void sched_init(void) {
    run_queue_head = NULL;
    run_queue_tail = NULL;
    kprintf("[sched] Scheduler initialized (round-robin)\n");
}


void sched_enqueue(process_t *proc) {
    proc->state = PROC_READY;
    proc->next = NULL;

    if (!run_queue_tail) {
        run_queue_head = proc;
        run_queue_tail = proc;
    } else {
        run_queue_tail->next = proc;
        run_queue_tail = proc;
    }
}


/*
 * schedule() — Pick the next process to run.
 *
 * Called from the timer interrupt handler. If there's a process
 * in the run queue, switch to it. The current process goes to
 * the back of the queue.
 */
void schedule(void) {
    process_t *current = proc_current();

    /* Nothing in the queue — keep running current */
    if (!run_queue_head) {
        return;
    }

    /* Dequeue the next process */
    process_t *next = run_queue_head;
    run_queue_head = next->next;
    if (!run_queue_head) {
        run_queue_tail = NULL;
    }
    next->next = NULL;

    /* If current is still runnable, put it back in the queue */
    if (current->state == PROC_RUNNING) {
        current->state = PROC_READY;
        sched_enqueue(current);
    }

    /* If next is the same as current, no switch needed */
    if (next == current) {
        current->state = PROC_RUNNING;
        return;
    }

    /* Perform the context switch */
    next->state = PROC_RUNNING;

    /* Update current process pointer before switching */
    extern void proc_set_current(process_t *p);
    proc_set_current(next);

    context_switch(&current->context, &next->context);
}


void yield(void) {
    schedule();
}
