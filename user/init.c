/*
 * init.c — PID 1, the first scheduled task.
 *
 * Runs as a kernel thread in EL1. User-space / SVC path is
 * deferred until Phase 5b; for now this just exercises the
 * cooperative scheduler.
 */

#include <stdint.h>

extern void kprintf(const char *fmt, ...);
extern void yield(void);

void user_main(void) {
    kprintf("[init] Hello from PID 1\n");

    for (int64_t i = 0; i < 5; i++) {
        kprintf("[init] tick %ld\n", i);
        yield();
    }

    kprintf("[init] done\n");
}
