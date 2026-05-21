# Makefile for STM32F103 Electronic Compass Firmware
#
# Toolchain: arm-none-eabi-gcc (GNU Arm Embedded)
# Target:    STM32F103C8T6 (Cortex-M3)
# QEMU:      stm32vldiscovery or netduinoplus2 machine
#
# Usage:
#   make              - build firmware (elf, bin, hex)
#   make qemu         - build and run in QEMU with semihosting
#   make qemu-gdb     - build and run in QEMU with GDB server on :1234
#   make clean        - remove build artifacts

# Toolchain
CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
AS      := $(CROSS)as
LD      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
SIZE    := $(CROSS)size

# Target
TARGET  := compass

# Sources
C_SRCS  := main.c \
           uart.c \
           i2c_driver.c \
           spi_driver.c \
           hmc5883l.c \
           mpu6050.c \
           ssd1306.c \
           madgwick.c

ASM_SRCS := startup.s

OBJS    := $(C_SRCS:.c=.o) $(ASM_SRCS:.s=.o)

# Flags
MCU_FLAGS   := -mcpu=cortex-m3 -mthumb
DEFINES     :=
INCLUDES    := -I.

# C flags
CFLAGS      := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) \
               -Wall -Wextra -Wno-unused-parameter \
               -ffunction-sections -fdata-sections \
               -fno-common -fmessage-length=0 \
               -std=gnu11 -Os -g3

# ASM flags
ASFLAGS     := $(MCU_FLAGS) -g

# Linker flags
LDFLAGS     := $(MCU_FLAGS) \
               -Wl,--gc-sections \
               -Wl,--print-memory-usage \
               -T linker.ld \
               -nostartfiles \
               -specs=nosys.specs \
               -specs=nano.specs \
               -Wl,-e,Reset_Handler \
               -lc -lm -lnosys \
               -Wl,-Map=$(TARGET).map

# QEMU configuration
QEMU        ?= qemu-system-arm
QEMU_MACHINE ?= stm32vldiscovery
QEMU_FLAGS  := -M $(QEMU_MACHINE) \
               -cpu cortex-m3 \
               -nographic \
               -serial mon:stdio \
               -kernel $(TARGET).elf

# Build rules
.PHONY: all clean qemu qemu-gdb disasm size

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex size

$(TARGET).elf: $(OBJS) linker.ld
	$(LD) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

disasm: $(TARGET).elf
	$(OBJDUMP) -d $< > $(TARGET).disasm

size: $(TARGET).elf
	$(SIZE) $<

# Run in QEMU with semihosting output
qemu: $(TARGET).elf
	@echo "Starting QEMU with semihosting..."
	@echo "Register verification results will appear below:"
	@echo "================================================"
	$(QEMU) $(QEMU_FLAGS) 2>&1 || true
	@echo "================================================"
	@echo "QEMU exited."

# Run in QEMU with GDB server (connect via: arm-none-eabi-gdb -ex 'target remote :1234')
qemu-gdb: $(TARGET).elf
	@echo "Starting QEMU with GDB server on port 1234..."
	@echo "In another terminal: arm-none-eabi-gdb $(TARGET).elf -ex 'target remote :1234'"
	$(QEMU) $(QEMU_FLAGS) -s -S

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).map $(TARGET).disasm
