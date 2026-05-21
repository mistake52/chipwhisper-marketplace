# Makefile for STM32F407VGT6 Discovery Board — LM75 I2C Temperature Sensor + LCD
# Register values from general knowledge — NOT MCP-verified.
# Verified via QEMU netduinoplus2 simulation.

TARGET = lm75_f407
CHIP  = STM32F407VG

# Toolchain
TC = arm-none-eabi-
CC      = $(TC)gcc
OBJCOPY = $(TC)objcopy
OBJDUMP = $(TC)objdump
SIZE    = $(TC)size

# CPU and FPU flags
CPU_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

# Simulation mode: set SIM=1 when building for QEMU verification
SIM ?= 0

# C flags
CFLAGS  = $(CPU_FLAGS) -O2 -g -Wall -Wextra
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DSTM32F407xx -DHSE_VALUE=8000000UL

ifeq ($(SIM),1)
CFLAGS += -DSIMULATION
endif

# Linker flags
LDFLAGS  = $(CPU_FLAGS) -T STM32F407VG.ld
LDFLAGS += -Wl,--gc-sections -Wl,-Map=$(TARGET).map
LDFLAGS += -nostartfiles -specs=nosys.specs -lm

# Source files
SRCDIR = src
SRCS   = $(wildcard $(SRCDIR)/*.c)
OBJS   = $(SRCS:.c=.o)

ASRC  = startup_stm32f407.s
AOBJ  = $(ASRC:.s=.o)

.PHONY: all clean flash

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(AOBJ) $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(AOBJ): $(ASRC)
	$(CC) $(CPU_FLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Flash using st-flash (ST-Link)
flash: $(TARGET).hex
	st-flash --format ihex write $(TARGET).hex

clean:
	rm -f $(TARGET).elf $(TARGET).hex $(TARGET).bin $(TARGET).map
	rm -f $(AOBJ) $(OBJS)
