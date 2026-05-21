# Makefile — MPU6050 driver for STM32F103C8T6
# Bare-metal, no HAL. Uses arm-none-eabi-gcc.

# Project name
TARGET = mpu6050_demo

# Toolchain
CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
LD      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE    = arm-none-eabi-size

# CPU flags (Cortex-M3, no FPU)
CPUFLAGS  = -mcpu=cortex-m3 -mthumb

# Compiler flags
CFLAGS    = $(CPUFLAGS) -O2 -g -Wall -Wextra
CFLAGS   += -ffunction-sections -fdata-sections
CFLAGS   += -DSTM32F103C8

# Linker flags
LDFLAGS   = $(CPUFLAGS) -T stm32f103c8.ld -Wl,--gc-sections -lm
LDFLAGS  += -specs=nosys.specs -specs=nano.specs

# Source files
C_SOURCES  = src/main.c
C_SOURCES += src/rcc.c
C_SOURCES += src/systick.c
C_SOURCES += src/uart.c
C_SOURCES += src/i2c_bus.c
C_SOURCES += src/mpu6050.c

ASM_SOURCES = startup_stm32f103c8.s

# Object files (auto-generate)
OBJS = $(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o)

# Include paths
INCLUDES = -Isrc

# Build rules
.PHONY: all clean flash size

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin size

$(TARGET).elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.s
	$(AS) $(CPUFLAGS) -c -o $@ $<

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

size: $(TARGET).elf
	$(SIZE) $(TARGET).elf

flash: $(TARGET).bin
	@echo "Flash the binary using ST-Link or DAPlink:"
	@echo "  st-flash write $(TARGET).bin 0x08000000"
	@echo "  or copy $(TARGET).bin to DAPlink virtual drive"

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).hex $(TARGET).bin
