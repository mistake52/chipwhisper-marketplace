##############################################################################
# Makefile for STM32F407VGT6 Discovery: LM75 + ILI9341 LCD
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc)
#   - STM32CubeF4 firmware package
#
# Usage:
#   make              - Build firmware (ELF, HEX, BIN)
#   make clean        - Remove build artifacts
#   make flash        - Flash to target using ST-LINK (requires st-flash)
#   make debug        - Start GDB server (requires st-util)
##############################################################################

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------
PREFIX    = arm-none-eabi-
CC        = $(PREFIX)gcc
CXX       = $(PREFIX)g++
AS        = $(PREFIX)gcc -x assembler-with-cpp
LD        = $(PREFIX)gcc
OBJCOPY   = $(PREFIX)objcopy
OBJDUMP   = $(PREFIX)objdump
SIZE      = $(PREFIX)size
GDB       = $(PREFIX)gdb

# ---------------------------------------------------------------------------
# STM32CubeF4 Paths (ADJUST THESE TO YOUR SYSTEM)
# ---------------------------------------------------------------------------
CUBE_FW   ?= /opt/STM32Cube_FW_F4_V1.27.0
HAL_DIR   ?= $(CUBE_FW)/Drivers/STM32F4xx_HAL_Driver
CMSIS_DIR ?= $(CUBE_FW)/Drivers/CMSIS

# If you have the HAL/CMSIS elsewhere, override CUBE_FW on the command line:
#   make CUBE_FW=/path/to/stm32cubef4

# ---------------------------------------------------------------------------
# Project Name & Output
# ---------------------------------------------------------------------------
TARGET    = lm75_lcd_f407
BUILD_DIR = build

# ---------------------------------------------------------------------------
# Source Files
# ---------------------------------------------------------------------------
C_SRCS  = main.c
C_SRCS += lm75.c
C_SRCS += lcd_ili9341.c

# HAL Driver source files (minimal set for GPIO, I2C, SPI, RCC, PWR, CORTEX)
HAL_SRCS  = stm32f4xx_hal.c
HAL_SRCS += stm32f4xx_hal_cortex.c
HAL_SRCS += stm32f4xx_hal_gpio.c
HAL_SRCS += stm32f4xx_hal_i2c.c
HAL_SRCS += stm32f4xx_hal_i2c_ex.c
HAL_SRCS += stm32f4xx_hal_rcc.c
HAL_SRCS += stm32f4xx_hal_rcc_ex.c
HAL_SRCS += stm32f4xx_hal_pwr.c
HAL_SRCS += stm32f4xx_hal_pwr_ex.c
HAL_SRCS += stm32f4xx_hal_spi.c
HAL_SRCS += stm32f4xx_hal_dma.c
HAL_SRCS += stm32f4xx_hal_exti.c
HAL_SRCS += stm32f4xx_hal_tim.c
HAL_SRCS += stm32f4xx_hal_flash.c
HAL_SRCS += stm32f4xx_hal_flash_ex.c

# CMSIS system files
CMSIS_SRCS  = system_stm32f4xx.c

# Startup file
ASM_SRCS  = startup_stm32f407xx.s

# ---------------------------------------------------------------------------
# Build object lists
# ---------------------------------------------------------------------------
C_OBJS  = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SRCS:.c=.o)))
HAL_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(HAL_SRCS:.c=.o)))
CMSIS_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(CMSIS_SRCS:.c=.o)))
ASM_OBJS  = $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SRCS:.s=.o)))

OBJS  = $(ASM_OBJS) $(CMSIS_OBJS) $(HAL_OBJS) $(C_OBJS)

# ---------------------------------------------------------------------------
# Compiler Flags
# ---------------------------------------------------------------------------
CPU       = -mcpu=cortex-m4 -mthumb
FPU       = -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# Include paths
INCLUDES  = -I.
INCLUDES += -I$(CMSIS_DIR)/Include
INCLUDES += -I$(CMSIS_DIR)/Device/ST/STM32F4xx/Include
INCLUDES += -I$(HAL_DIR)/Inc

# Defines
DEFINES  = -DUSE_HAL_DRIVER
DEFINES += -DSTM32F407xx
DEFINES += -DHSE_VALUE=8000000

# Optimization and debug
OPT       = -Og -g3
# OPT       = -Os -g   # Use for production

# Warnings
WARNINGS  = -Wall -Wextra -Wshadow -Wdouble-promotion
WARNINGS += -Wno-unused-parameter -Wno-sign-compare

# Combined C flags
CFLAGS  = $(CPU) $(FPU) $(OPT) $(WARNINGS)
CFLAGS += $(DEFINES) $(INCLUDES)
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -std=gnu11 -fno-common

# Combined C++ flags (not used but provided for completeness)
CXXFLAGS = $(CPU) $(FPU) $(OPT) $(WARNINGS)
CXXFLAGS += $(DEFINES) $(INCLUDES)
CXXFLAGS += -fdata-sections -ffunction-sections
CXXFLAGS += -std=gnu++17 -fno-common -fno-exceptions -fno-rtti

# Assembler flags
ASFLAGS = $(CPU) $(FPU) $(DEFINES) $(INCLUDES)
ASFLAGS += -Wa,-adhlns=$(BUILD_DIR)/$(notdir $(<:.s=.lst))

# ---------------------------------------------------------------------------
# Linker Flags
# ---------------------------------------------------------------------------
LDSCRIPT  = STM32F407VGTx_FLASH.ld

LIBS      = -lc -lm -lnosys
LIBDIR    =
LDFLAGS   = $(CPU) $(FPU) -T$(LDSCRIPT)
LDFLAGS  += $(LIBDIR) $(LIBS)
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
LDFLAGS  += -specs=nano.specs -specs=nosys.specs
LDFLAGS  += -Wl,--print-memory-usage

# ---------------------------------------------------------------------------
# Default target
# ---------------------------------------------------------------------------
.PHONY: all clean flash debug

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ---------------------------------------------------------------------------
# Application sources (in current directory)
# ---------------------------------------------------------------------------
$(BUILD_DIR)/main.o: main.c lm75.h lcd_ili9341.h stm32f4xx_hal_conf.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lm75.o: lm75.c lm75.h stm32f4xx_hal_conf.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lcd_ili9341.o: lcd_ili9341.c lcd_ili9341.h stm32f4xx_hal_conf.h
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# HAL sources (from STM32CubeF4)
# ---------------------------------------------------------------------------
$(BUILD_DIR)/stm32f4xx_hal.o: $(HAL_DIR)/Src/stm32f4xx_hal.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_cortex.o: $(HAL_DIR)/Src/stm32f4xx_hal_cortex.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_gpio.o: $(HAL_DIR)/Src/stm32f4xx_hal_gpio.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_i2c.o: $(HAL_DIR)/Src/stm32f4xx_hal_i2c.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_i2c_ex.o: $(HAL_DIR)/Src/stm32f4xx_hal_i2c_ex.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_rcc.o: $(HAL_DIR)/Src/stm32f4xx_hal_rcc.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_rcc_ex.o: $(HAL_DIR)/Src/stm32f4xx_hal_rcc_ex.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_pwr.o: $(HAL_DIR)/Src/stm32f4xx_hal_pwr.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_pwr_ex.o: $(HAL_DIR)/Src/stm32f4xx_hal_pwr_ex.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_spi.o: $(HAL_DIR)/Src/stm32f4xx_hal_spi.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_dma.o: $(HAL_DIR)/Src/stm32f4xx_hal_dma.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_exti.o: $(HAL_DIR)/Src/stm32f4xx_hal_exti.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_tim.o: $(HAL_DIR)/Src/stm32f4xx_hal_tim.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_flash.o: $(HAL_DIR)/Src/stm32f4xx_hal_flash.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f4xx_hal_flash_ex.o: $(HAL_DIR)/Src/stm32f4xx_hal_flash_ex.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# CMSIS system source
# ---------------------------------------------------------------------------
$(BUILD_DIR)/system_stm32f4xx.o: $(CMSIS_DIR)/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# Startup assembly
# ---------------------------------------------------------------------------
$(BUILD_DIR)/startup_stm32f407xx.o: startup_stm32f407xx.s
	$(AS) $(ASFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# Link
# ---------------------------------------------------------------------------
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@$(SIZE) $@

# ---------------------------------------------------------------------------
# Generate HEX and BIN files for flashing
# ---------------------------------------------------------------------------
$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------
clean:
	rm -rf $(BUILD_DIR)

# ---------------------------------------------------------------------------
# Flash using ST-LINK (requires stlink-tools)
# ---------------------------------------------------------------------------
flash: $(BUILD_DIR)/$(TARGET).bin
	@echo "Flashing $(TARGET).bin to STM32F407VGT6..."
	st-flash --reset write $(BUILD_DIR)/$(TARGET).bin 0x08000000

# ---------------------------------------------------------------------------
# Debug server (requires stlink-tools)
# ---------------------------------------------------------------------------
debug:
	@echo "Starting GDB server on :4242..."
	st-util -p 4242

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------
help:
	@echo "Targets:"
	@echo "  all      - Build firmware (ELF, HEX, BIN)"
	@echo "  clean    - Remove all build artifacts"
	@echo "  flash    - Flash to target via ST-LINK"
	@echo "  debug    - Start ST-LINK GDB server on :4242"
	@echo ""
	@echo "Variables:"
	@echo "  CUBE_FW  - Path to STM32CubeF4 package"
	@echo "              (default: /opt/STM32Cube_FW_F4_V1.27.0)"
	@echo ""
	@echo "Example:"
	@echo "  make CUBE_FW=~/STM32Cube/Repository/STM32Cube_FW_F4_V1.27.0"
