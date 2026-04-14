# ArmOS Makefile
#
# Cross-compiles the kernel for AArch64 bare-metal and runs it in QEMU.

# ─── Toolchain ────────────────────────────────────────────────
# Try aarch64-none-elf first (macOS Homebrew), fall back to aarch64-linux-gnu (Linux)
ifneq ($(shell which aarch64-none-elf-gcc 2>/dev/null),)
    CROSS = aarch64-none-elf-
else ifneq ($(shell which aarch64-elf-gcc 2>/dev/null),)
    CROSS = aarch64-elf-
else
    CROSS = aarch64-linux-gnu-
endif

CC      = $(CROSS)gcc
AS      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

# ─── Flags ────────────────────────────────────────────────────
# -ffreestanding:  No standard library, no startup files
# -nostdlib:       Don't link libc
# -mcpu=cortex-a72: Target the Cortex-A72 (QEMU virt default)
# -Wall -Wextra:   Catch bugs early
CFLAGS  = -ffreestanding -nostdlib -mcpu=cortex-a72 -Wall -Wextra -O2 -Iinclude
ASFLAGS = -mcpu=cortex-a72
LDFLAGS = -T boot/linker.ld -nostdlib

# Debug build: no optimization, include debug symbols
DEBUG_CFLAGS = -ffreestanding -nostdlib -mcpu=cortex-a72 -Wall -Wextra -O0 -g -Iinclude

# ─── QEMU ─────────────────────────────────────────────────────
QEMU    = qemu-system-aarch64
QFLAGS  = -machine virt,gic-version=2 -cpu cortex-a72 -m 1G -nographic -kernel

# ─── Sources ──────────────────────────────────────────────────
ASM_SRCS = boot/boot.S arch/aarch64/vectors.S arch/aarch64/context.S \
           user/libc/syscall.S
C_SRCS   = kernel/main.c kernel/panic.c drivers/uart.c drivers/gic.c \
           arch/aarch64/exception.c arch/aarch64/timer.c \
           mm/pmm.c mm/vmm.c mm/kmalloc.c \
           proc/process.c proc/scheduler.c proc/syscall.c \
           user/init.c user/libc/stdio.c

ASM_OBJS = $(patsubst %.S, build/%.o, $(ASM_SRCS))
C_OBJS   = $(patsubst %.c, build/%.o, $(C_SRCS))
OBJS     = $(ASM_OBJS) $(C_OBJS)

# ─── Output ───────────────────────────────────────────────────
KERNEL_ELF = build/kernel.elf
KERNEL_BIN = build/kernel.bin

# ─── Targets ──────────────────────────────────────────────────
.PHONY: all clean run debug debug-run

all: $(KERNEL_BIN)

# Link all objects into ELF
$(KERNEL_ELF): $(OBJS)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -o $@ $^

# Convert ELF to raw binary (what QEMU loads)
$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "  OBJCOPY $@"
	@$(OBJCOPY) -O binary $< $@
	@echo ""
	@echo "  Build complete: $@ ($(shell wc -c < $@ | tr -d ' ') bytes)"
	@echo ""

# Compile C sources
build/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assemble ASM sources
build/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

# Debug build (rebuild with debug flags)
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: clean all

# ─── Run ──────────────────────────────────────────────────────
run: $(KERNEL_BIN)
	@echo "  QEMU    Starting ArmOS..."
	@echo "  (Press Ctrl+A then X to exit QEMU)"
	@echo ""
	@$(QEMU) $(QFLAGS) $(KERNEL_BIN)

# Run with GDB server (connect with: aarch64-none-elf-gdb build/kernel.elf)
debug-run: debug
	@echo "  QEMU    Starting ArmOS (GDB on :1234)..."
	@echo "  (Connect: target remote :1234)"
	@echo ""
	@$(QEMU) $(QFLAGS) $(KERNEL_ELF) -S -gdb tcp::1234

# ─── Clean ────────────────────────────────────────────────────
clean:
	@echo "  CLEAN"
	@rm -rf build/
