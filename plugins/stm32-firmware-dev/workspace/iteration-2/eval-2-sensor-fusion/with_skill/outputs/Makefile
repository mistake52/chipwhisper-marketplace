# Makefile — STM32F103C8 E-Compass firmware
#
# Build targets:
#   make all              Hardware: HSE → PLL×9 → 72MHz
#   make SIMULATION=1 all QEMU: HSI 8MHz, no PLL
#   make clean
#
# Prerequisites: arm-none-eabi-gcc (>= 13.x)

# Toolchain
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE    = arm-none-eabi-size

# Project name
TARGET  = ecompass_f103

# Directories
SRCDIR  = src
BUILDDIR = build

# Simulation flag — uses 8KB RAM linker for QEMU (F100RBT6)
ifdef SIMULATION
CFLAGS += -DSIMULATION
LDSCRIPT = stm32f103c8_sim.ld
else
LDSCRIPT = stm32f103c8.ld
endif

# Compiler flags
CFLAGS  += -mcpu=cortex-m3 -mthumb
CFLAGS  += -O2 -g -Wall -Wextra
CFLAGS  += -ffunction-sections -fdata-sections
CFLAGS  += -fno-common
CFLAGS  += -I$(SRCDIR)

# Linker flags
LDFLAGS  = -mcpu=cortex-m3 -mthumb
LDFLAGS += -T $(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILDDIR)/$(TARGET).map
LDFLAGS += --specs=nosys.specs
LDLIBS   = -lm   # Software floating-point math library (Cortex-M3 has no FPU)

# Source files
C_SRCS  = $(wildcard $(SRCDIR)/*.c)
ASM_SRCS = startup_stm32f103c8.s

# Object files
C_OBJS  = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(C_SRCS))
ASM_OBJS = $(patsubst %.s,$(BUILDDIR)/%.o,$(ASM_SRCS))
OBJS     = $(C_OBJS) $(ASM_OBJS)

.PHONY: all clean flash

all: $(BUILDDIR)/$(TARGET).elf $(BUILDDIR)/$(TARGET).hex $(BUILDDIR)/$(TARGET).bin

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile C sources
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble startup file
$(BUILDDIR)/%.o: %.s | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(BUILDDIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
	$(SIZE) $@

# Generate hex and bin
$(BUILDDIR)/%.hex: $(BUILDDIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILDDIR)/%.bin: $(BUILDDIR)/%.elf
	$(OBJCOPY) -O binary $< $@

# Flash via ST-Link
flash: $(BUILDDIR)/$(TARGET).bin
	st-flash write $< 0x08000000

# Clean
clean:
	rm -rf $(BUILDDIR)
