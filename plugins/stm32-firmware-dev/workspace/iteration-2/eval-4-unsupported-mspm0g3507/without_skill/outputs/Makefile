# Makefile for MSPM0G3507 Temperature-Controlled Fan Firmware
#
# Requirements: arm-none-eabi-gcc toolchain
#   Ubuntu/Debian: sudo apt install gcc-arm-none-eabi
#   Arch:           sudo pacman -S arm-none-eabi-gcc
#   macOS:          brew install arm-none-eabi-gcc

# Toolchain prefix
PREFIX   = arm-none-eabi-

CC       = $(PREFIX)gcc
OBJCOPY  = $(PREFIX)objcopy
OBJDUMP  = $(PREFIX)objdump
SIZE     = $(PREFIX)size

# MCU-specific flags
MCU      = cortex-m0plus
CPUFLAGS = -mcpu=$(MCU) -mthumb -mfloat-abi=soft

# Compiler flags
CFLAGS   = $(CPUFLAGS)                       \
           -Wall -Wextra                     \
           -O2                               \
           -g3                               \
           -ffunction-sections               \
           -fdata-sections                   \
           -fno-common                       \
           -fno-builtin                      \
           -std=c11                          \
           -nostdlib                         \
           -ffreestanding

# Linker flags
LDFLAGS  = $(CPUFLAGS)                       \
           -T mspm0g3507.ld                  \
           -nostartfiles                     \
           -nostdlib                         \
           -Wl,--gc-sections                 \
           -Wl,-Map=build/firmware.map       \
           --specs=nosys.specs

# Source files
SRCS     = main.c startup_mspm0g3507.c
OBJS     = $(SRCS:%.c=build/%.o)

# Targets
TARGET   = build/firmware

# ------------------------------------------------------------------------
# Default target
# ------------------------------------------------------------------------
.PHONY: all
all: dirs $(TARGET).elf $(TARGET).bin $(TARGET).hex size

# ------------------------------------------------------------------------
# Create build directory
# ------------------------------------------------------------------------
.PHONY: dirs
dirs:
	@mkdir -p build

# ------------------------------------------------------------------------
# Compile: C -> object
# ------------------------------------------------------------------------
build/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ------------------------------------------------------------------------
# Link: objects -> ELF
# ------------------------------------------------------------------------
$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

# ------------------------------------------------------------------------
# objcopy: ELF -> binary
# ------------------------------------------------------------------------
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

# ------------------------------------------------------------------------
# objcopy: ELF -> Intel HEX
# ------------------------------------------------------------------------
%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

# ------------------------------------------------------------------------
# Disassembly (for debugging)
# ------------------------------------------------------------------------
.PHONY: disasm
disasm: $(TARGET).elf
	$(OBJDUMP) -d $< > build/firmware.disasm

# ------------------------------------------------------------------------
# Size report
# ------------------------------------------------------------------------
.PHONY: size
size: $(TARGET).elf
	$(SIZE) --format=berkeley $<

# ------------------------------------------------------------------------
# Clean build artifacts
# ------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -rf build

# ------------------------------------------------------------------------
# Flash the device (using TI XDS110 or CMSIS-DAP debug probe)
#
# Options:
#   A) OpenOCD:
#        openocd -f board/ti_mspm0g3507.cfg  -c "program build/firmware.elf verify reset exit"
#   B) J-Link:
#        JLinkExe -device MSPM0G3507 -if SWD -speed 4000 -autoconnect 1 -CommanderScript flash.jlink
#   C) pyOCD:
#        pyocd flash -t mspm0g3507 build/firmware.elf
# ------------------------------------------------------------------------
.PHONY: flash
flash: $(TARGET).elf
	@echo "Flash commands:"
	@echo "  openocd -f board/ti_mspm0g3507.cfg -c \"program build/firmware.elf verify reset exit\""
	@echo "  pyocd flash -t mspm0g3507 build/firmware.elf"
	@echo ""
	@echo "Uncomment the appropriate command in the Makefile for your debug probe."
