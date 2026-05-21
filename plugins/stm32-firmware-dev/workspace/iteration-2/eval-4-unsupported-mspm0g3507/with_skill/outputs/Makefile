# Makefile — MSPM0G3507 Temperature-Controlled Fan
#
# Requires: arm-none-eabi-gcc toolchain
#   Ubuntu/Debian: sudo apt install gcc-arm-none-eabi
#   macOS:         brew install arm-none-eabi-gcc
#
# MANUAL ONLY: No MCP verification or QEMU simulation available for MSPM0G3507.
# Build artifacts can only be flashed to real hardware.

PROJECT   := temp_fan
CHIP      := mspm0g3507

# Toolchain
CC        := arm-none-eabi-gcc
AS        := arm-none-eabi-as
LD        := arm-none-eabi-ld
OBJCOPY   := arm-none-eabi-objcopy
SIZE      := arm-none-eabi-size

# Directories
SRCDIR    := src
BUILDDIR  := build

# Sources
C_SRCS    := $(wildcard $(SRCDIR)/*.c)
ASM_SRCS  := startup_mspm0g3507.s

# Objects
C_OBJS    := $(C_SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
ASM_OBJS  := $(ASM_SRCS:%.s=$(BUILDDIR)/%.o)
OBJS      := $(ASM_OBJS) $(C_OBJS)

# Flags
# Cortex-M0+ does not have hardware divide (no -mcpu=cortex-m0plus divides), so
# we link with libgcc (-lgcc) for software integer division.
CPUFLAGS  := -mcpu=cortex-m0plus -mthumb
CFLAGS    := $(CPUFLAGS) -O2 -g -Wall -Wextra -ffunction-sections -fdata-sections
CFLAGS    += -nostdlib -ffreestanding
CFLAGS    += -I$(SRCDIR)
LDFLAGS   := -T $(CHIP).ld -nostdlib -lgcc -Wl,--gc-sections

# Target: build all
.PHONY: all
all: $(BUILDDIR)/$(PROJECT).elf $(BUILDDIR)/$(PROJECT).hex $(BUILDDIR)/$(PROJECT).bin
	@echo "=== Build complete ==="
	@$(SIZE) $(BUILDDIR)/$(PROJECT).elf

# ELF (link)
$(BUILDDIR)/$(PROJECT).elf: $(OBJS) $(CHIP).ld
	@mkdir -p $(BUILDDIR)
	$(CC) $(CPUFLAGS) $(OBJS) $(LDFLAGS) -o $@

# Intel HEX
$(BUILDDIR)/$(PROJECT).hex: $(BUILDDIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

# Raw binary
$(BUILDDIR)/$(PROJECT).bin: $(BUILDDIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

# Compile C
$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/main.h
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble startup
$(BUILDDIR)/%.o: %.s
	@mkdir -p $(BUILDDIR)
	$(CC) $(CPUFLAGS) -c $< -o $@

# Flash via TI XDS110 / CMSIS-DAP (LP-MSPM0G3507 LaunchPad)
.PHONY: flash
flash: $(BUILDDIR)/$(PROJECT).elf
	@echo "Flashing via OpenOCD (CMSIS-DAP)..."
	openocd -f board/ti_mspm0g3507.cfg \
	        -c "program $(BUILDDIR)/$(PROJECT).elf verify reset exit"

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILDDIR)

# Disassembly (for manual inspection)
.PHONY: disasm
disasm: $(BUILDDIR)/$(PROJECT).elf
	arm-none-eabi-objdump -d $< | less
