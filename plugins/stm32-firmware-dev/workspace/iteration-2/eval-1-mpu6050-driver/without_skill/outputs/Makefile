# Makefile — STM32F103C8T6 + MPU6050 Bare-Metal Firmware
# =======================================================
# Toolchain: arm-none-eabi-gcc
# Outputs: firmware.elf, firmware.bin, firmware.hex

PROJECT   = firmware
TARGET    = stm32f103c8t6

# Toolchain
CC        = arm-none-eabi-gcc
OBJCOPY   = arm-none-eabi-objcopy
SIZE      = arm-none-eabi-size

# Sources
SRCS      = main.c mpu6050.c
OBJS      = $(SRCS:.c=.o)

# Flags
CPUFLAGS  = -mcpu=cortex-m3 -mthumb
CFLAGS    = $(CPUFLAGS) -Wall -Wextra -O2 -g
CFLAGS   += -ffunction-sections -fdata-sections
CFLAGS   += -fno-common -fno-builtin
LDFLAGS   = $(CPUFLAGS) -T stm32f103c8t6.ld
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -nostartfiles -nostdlib -lc -lgcc
LDFLAGS  += -Wl,-Map=$(PROJECT).map,--cref

# ======================================================================
# Targets
# ======================================================================

.PHONY: all clean flash

all: $(PROJECT).elf $(PROJECT).bin $(PROJECT).hex
	@$(SIZE) $(PROJECT).elf

$(PROJECT).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

# ----------------------------------------------------------------------
# Flash via openocd (ST-Link / V2)
# ----------------------------------------------------------------------
flash: $(PROJECT).bin
	openocd -f interface/stlink.cfg \
	        -f target/stm32f1x.cfg \
	        -c "program $(PROJECT).bin 0x08000000 verify reset exit"

# ----------------------------------------------------------------------
# Erase chip
# ----------------------------------------------------------------------
erase:
	openocd -f interface/stlink.cfg \
	        -f target/stm32f1x.cfg \
	        -c "init; reset halt; stm32f1x mass_erase 0; exit"

# ----------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------
clean:
	rm -f $(OBJS) $(PROJECT).elf $(PROJECT).bin $(PROJECT).hex $(PROJECT).map
