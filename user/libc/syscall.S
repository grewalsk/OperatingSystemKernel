/*
 * syscall.S — User-space system call stubs
 *
 * Each stub loads the syscall number into x8 and issues SVC #0.
 * Arguments are already in x0-x5 per the C calling convention.
 * Return value comes back in x0.
 */

.global sys_write
.global sys_exit
.global sys_yield
.global sys_getpid

/* ssize_t write(int fd, const void *buf, size_t count) */
sys_write:
    mov     x8, #1          /* SYS_WRITE */
    svc     #0
    ret

/* void exit(int status) */
sys_exit:
    mov     x8, #2          /* SYS_EXIT */
    svc     #0
    ret                     /* Should never reach here */

/* void yield(void) */
sys_yield:
    mov     x8, #3          /* SYS_YIELD */
    svc     #0
    ret

/* int getpid(void) */
sys_getpid:
    mov     x8, #4          /* SYS_GETPID */
    svc     #0
    ret
