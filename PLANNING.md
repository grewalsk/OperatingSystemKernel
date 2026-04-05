# ArmOS — Planning Document

> **Type:** Application (adapted for systems programming)  
> **Stack:** C + AArch64 Assembly | QEMU `virt` machine | `aarch64-none-elf-gcc`  
> **Quality Gates:** Boots in QEMU, passes unit tests, no undefined behavior (checked via `-fsanitize` where applicable)

---

## Problem Statement

Build an operating system kernel from scratch targeting AArch64 to deeply understand how operating systems work at every layer — boot, memory, processes, syscalls, and filesystems. This is a learning-first project: clarity and understanding take priority over performance or feature count.

**Audience:** The builder (educational/self-directed learning).  
**Success criteria:** A kernel that boots on QEMU, manages memory, schedules processes, handles system calls, and runs user-space programs with a shell.

---

## Tech Stack

| Component | Choice | Why |
|-----------|--------|-----|
| **Language** | C17 + AArch64 Assembly | Industry standard for kernels, direct hardware access |
| **Target** | AArch64 (ARMv8-A, 64-bit) | Modern, clean ISA, industry-relevant |
| **Platform** | QEMU `virt` machine | No real hardware needed, great debugging support |
| **CPU Model** | Cortex-A72 (emulated) | Common, well-documented |
| **Compiler** | `aarch64-none-elf-gcc` | Bare-metal cross-compiler, no OS assumptions |
| **Build** | GNU Make | Simple, universal, no extra dependencies |
| **Debugger** | GDB via QEMU `-gdb` flag | Step through kernel code, inspect registers |
| **Version Control** | Git + GitHub | Standard workflow |

---

## Architecture Decisions

### Memory Model
- **Physical allocator:** Bitmap-based page allocator (simple, easy to debug)
- **Virtual memory:** 4-level page tables, 4KB granule, 48-bit virtual addresses
- **Kernel mapping:** Higher-half kernel (addresses above `0xFFFF000000000000`)
- **User mapping:** Lower-half (addresses below `0x0000FFFFFFFFFFFF`)
- **Two TTBR registers:** `TTBR1_EL1` for kernel, `TTBR0_EL1` for user (hardware-supported split)

### Process Model
- **Scheduling:** Round-robin with timer preemption (simple, fair)
- **Max processes:** 64 initially (fixed table, upgradeable to dynamic)
- **Context switch:** Full register save/restore in assembly
- **User/kernel boundary:** `SVC` instruction for syscalls, `ERET` for return

### Interrupt Model
- **Controller:** GICv3 (QEMU virt default)
- **Timer:** ARM Generic Timer (`CNTPCT_EL0` / `CNTP_TVAL_EL0`)
- **UART:** PL011 at `0x09000000` (QEMU virt fixed address)

### Filesystem
- **Type:** RAM-based filesystem (no disk, no persistence)
- **VFS layer:** Minimal abstraction to allow multiple FS types (`ramfs`, `devfs`)
- **Initial ramdisk:** Linked into kernel image at build time

---

## Phase Breakdown

### Phase 1: Boot & UART
**Goal:** Get to a C `kernel_main()` and print to serial console.

**Deliverables:**
- `boot/boot.S` — AArch64 startup: park secondary cores, set SP, zero BSS, branch to C
- `boot/linker.ld` — Linker script placing kernel at `0x40000000`
- `drivers/uart.c` — PL011 UART init + `putc`/`puts`/`printf`
- `kernel/main.c` — `kernel_main()` prints "Hello, kernel!"
- `Makefile` — Cross-compile, link, run in QEMU

**Done when:** `make run` shows "Hello, kernel!" in terminal.

**Key learnings:** Cross-compilation, linker scripts, MMIO, bare-metal startup.

---

### Phase 2: Interrupts & Exceptions
**Goal:** Handle hardware interrupts and CPU exceptions.

**Deliverables:**
- `arch/aarch64/exception.S` — Exception vector table (aligned to 2048 bytes)
- `arch/aarch64/exception.c` — IRQ dispatch, exception info printing
- `drivers/gic.c` — GICv3 initialization, enable/disable IRQs
- `arch/aarch64/timer.c` — Configure ARM Generic Timer, periodic tick
- `kernel/panic.c` — Kernel panic with register dump

**Done when:** Timer interrupt fires every 1 second and prints a tick count.

**Key learnings:** Exception levels, vector tables, interrupt controllers, MMIO registers.

---

### Phase 3: Memory Management
**Goal:** Physical page allocation and virtual memory via MMU.

**Deliverables:**
- `mm/pmm.c` — Bitmap-based physical page allocator
- `mm/vmm.c` — Page table creation, mapping/unmapping pages
- `arch/aarch64/mmu.c` — MMU enable, TTBR setup, TLB management
- `mm/kmalloc.c` — Simple kernel heap allocator (bump or free-list)
- Update `kernel/main.c` — Enable MMU during boot, remap kernel to higher-half

**Done when:** Kernel runs with MMU enabled, allocates/frees pages, `kmalloc`/`kfree` work.

**Key learnings:** Page tables, TLB, virtual address spaces, memory protection.

---

### Phase 4: Process & Scheduling
**Goal:** Multiple processes running concurrently with preemptive scheduling.

**Deliverables:**
- `proc/process.c` — PCB, process creation (`create_process`), destruction
- `arch/aarch64/context.S` — Register save/restore for context switch
- `proc/scheduler.c` — Round-robin scheduler, run queue
- Update timer handler — Call `schedule()` on each tick
- Create 2+ kernel tasks that print alternately

**Done when:** Two tasks print interleaved output driven by timer preemption.

**Key learnings:** Context switching, scheduling algorithms, concurrency at the hardware level.

---

### Phase 5: System Calls & User Space
**Goal:** Run code in EL0 (user mode) that communicates with the kernel via syscalls.

**Deliverables:**
- `proc/syscall.c` — Syscall table, dispatch from `SVC` exception
- `user/libc/syscall.S` — User-space syscall stubs (`SVC #0`)
- `user/libc/stdio.c` — `printf`, `puts` using `write` syscall
- `user/init.c` — First user-space process (PID 1)
- Update process creation — Set up EL0 return, user stack, `TTBR0`

**Done when:** A user-space program calls `write()` and output appears on the UART console.

**Key learnings:** Privilege levels, system call ABI, user/kernel isolation.

---

### Phase 6: Filesystem & Shell
**Goal:** Load and run programs from a filesystem, interactive shell.

**Deliverables:**
- `fs/vfs.c` — VFS layer with `open`/`read`/`write`/`close`
- `fs/ramfs.c` — RAM-based filesystem implementation
- `fs/devfs.c` — `/dev/console` backed by UART
- ELF loader — Parse ELF headers, load segments, set entry point
- `user/shell.c` — Read input, parse command, fork+exec
- `user/libc/string.c` — `strcmp`, `strlen`, `memcpy`

**Done when:** Shell prompt appears, user types a command, program runs and exits.

**Key learnings:** Filesystems, ELF format, program loading, the exec model.

---

## Security Considerations

Even for a learning OS, good habits matter:
- **Kernel/user isolation:** MMU enforced, user pages marked `EL0` accessible only
- **No execute from user pages in kernel mode:** `PXN` bit set on user mappings
- **Stack guard pages:** Unmapped page below each stack to catch overflow
- **Syscall validation:** Check all user pointers before dereferencing in kernel

---

## Skill Loadout

| Tool | Purpose | When |
|------|---------|------|
| **PAUL** | Managed build workflow for each phase | During implementation |
| **AEGIS** | Security audit of kernel code | Post-phase 5 (after syscalls exist) |

---

## Open Questions

- [ ] Should we support SMP (multi-core) as a stretch goal?
- [ ] Worth adding a simple networking stack (virtio-net)?
- [ ] Should the shell support pipes/redirection?

---

*Generated by SEED — project ideation tool*
