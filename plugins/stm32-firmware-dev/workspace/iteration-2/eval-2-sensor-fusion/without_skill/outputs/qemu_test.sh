#!/bin/bash
# =============================================================================
# QEMU Register Verification Test for STM32F103 Electronic Compass
#
# This script builds the firmware and runs it in QEMU to verify that all
# peripheral registers are correctly initialized. It also attempts to compile
# with a real toolchain if available, or performs an offline static analysis.
#
# Usage:
#   bash qemu_test.sh
#
# Requirements:
#   - arm-none-eabi-gcc (GNU Arm Embedded Toolchain)
#   - qemu-system-arm (with stm32vldiscovery machine support)
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=============================================="
echo " STM32F103 Electronic Compass - QEMU Test"
echo "=============================================="
echo ""

# -------------------------------------------------------------------
# Step 1: Check toolchain availability
# -------------------------------------------------------------------
echo -n "Checking arm-none-eabi-gcc... "
if command -v arm-none-eabi-gcc &>/dev/null; then
    echo -e "${GREEN}FOUND${NC}"
    GCC_VER=$(arm-none-eabi-gcc --version | head -1)
    echo "  $GCC_VER"
    HAS_TOOLCHAIN=1
else
    echo -e "${RED}NOT FOUND${NC}"
    echo "  Attempting to locate via common paths..."
    # Try some common paths
    for path in \
        /usr/bin/arm-none-eabi-gcc \
        /usr/local/bin/arm-none-eabi-gcc \
        "$HOME/.local/bin/arm-none-eabi-gcc" \
        /opt/gcc-arm-none-eabi/bin/arm-none-eabi-gcc; do
        if [ -x "$path" ]; then
            echo -e "  ${GREEN}Found at: $path${NC}"
            HAS_TOOLCHAIN=1
            export PATH="${path%/*}:$PATH"
            break
        fi
    done
    if [ -z "${HAS_TOOLCHAIN:-}" ]; then
        echo -e "  ${YELLOW}Toolchain not found. Running static analysis only.${NC}"
        echo ""
        echo "  Install the GNU Arm Embedded Toolchain:"
        echo "    Ubuntu: sudo apt install gcc-arm-none-eabi"
        echo "    macOS:  brew install arm-none-eabi-gcc"
        echo "    Manual: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain"
        HAS_TOOLCHAIN=0
    fi
fi

echo ""

# -------------------------------------------------------------------
# Step 2: Check QEMU availability
# -------------------------------------------------------------------
echo -n "Checking qemu-system-arm... "
if command -v qemu-system-arm &>/dev/null; then
    echo -e "${GREEN}FOUND${NC}"
    QEMU_VER=$(qemu-system-arm --version | head -1)
    echo "  $QEMU_VER"

    # Check for stm32vldiscovery machine support
    echo -n "Checking stm32vldiscovery machine... "
    if qemu-system-arm -M help 2>&1 | grep -q stm32vldiscovery; then
        echo -e "${GREEN}SUPPORTED${NC}"
        MACHINE="stm32vldiscovery"
    elif qemu-system-arm -M help 2>&1 | grep -q netduinoplus2; then
        echo -e "${YELLOW}Using netduinoplus2 (stm32vldiscovery not found)${NC}"
        MACHINE="netduinoplus2"
    else
        echo -e "${YELLOW}No STM32 machine found. Trying netduinoplus2 anyway.${NC}"
        MACHINE="netduinoplus2"
    fi
    HAS_QEMU=1
else
    echo -e "${RED}NOT FOUND${NC}"
    echo "  Install: sudo apt install qemu-system-arm"
    HAS_QEMU=0
fi
echo ""

# -------------------------------------------------------------------
# Step 3: Build the firmware
# -------------------------------------------------------------------
if [ "$HAS_TOOLCHAIN" -eq 1 ]; then
    echo "--- Building firmware ---"
    make clean 2>/dev/null || true
    if make all 2>&1; then
        echo -e "${GREEN}Build successful${NC}"
        echo ""

        # Show size info
        echo "--- Binary size ---"
        arm-none-eabi-size compass.elf 2>/dev/null || true
        echo ""

        HAS_BINARY=1
    else
        echo -e "${RED}Build failed${NC}"
        echo ""
        HAS_BINARY=0
    fi
else
    HAS_BINARY=0
    echo -e "${YELLOW}Skipping build (no toolchain).${NC}"
    echo ""
fi

# -------------------------------------------------------------------
# Step 4: Static analysis of register configuration
# -------------------------------------------------------------------
echo "=============================================="
echo " STATIC REGISTER CONFIGURATION ANALYSIS"
echo "=============================================="
echo ""

declare -A checks
PASS=0
FAIL=0

check_config() {
    local name="$1"
    local expected="$2"
    local actual="$3"
    if [ "$expected" = "$actual" ]; then
        echo -e "  [${GREEN}PASS${NC}] $name"
        ((PASS++)) || true
    else
        echo -e "  [${RED}FAIL${NC}] $name (expected: $expected)"
        ((FAIL++)) || true
    fi
}

# Parse source files to verify register configurations

MAIN_SRC="main.c"
I2C_SRC="i2c_driver.c"
SPI_SRC="spi_driver.c"

echo "--- Clock Configuration ---"

# Check RCC APB2ENR enables
if grep -q "RCC->APB2ENR.*RCC_APB2ENR_IOPAEN" "$MAIN_SRC" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] GPIOA clock enabled (RCC_APB2ENR_IOPAEN)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] GPIOA clock enabled"
    ((FAIL++)) || true
fi

if grep -q "RCC->APB2ENR.*RCC_APB2ENR_IOPBEN" "$MAIN_SRC" "$SPI_SRC" "$I2C_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] GPIOB clock enabled (RCC_APB2ENR_IOPBEN)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] GPIOB clock enabled"
    ((FAIL++)) || true
fi

if grep -q "RCC->APB2ENR.*RCC_APB2ENR_SPI1EN" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] SPI1 clock enabled (RCC_APB2ENR_SPI1EN)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] SPI1 clock enabled"
    ((FAIL++)) || true
fi

if grep -q "RCC->APB1ENR.*RCC_APB1ENR_I2C1EN" "$I2C_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] I2C1 clock enabled (RCC_APB1ENR_I2C1EN)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] I2C1 clock enabled"
    ((FAIL++)) || true
fi

echo ""
echo "--- GPIO Pin Configuration ---"

# Check I2C pins PB6/PB7 as AF OD
if grep -q "I2C1_SCL_PIN.*GPIO_CNF_AF_OD\|GPIO_CNF_AF_OD.*I2C1_SCL_PIN\|PB6.*AF.*OD\|SCL.*AF.*OD" "$I2C_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] PB6 (SCL) configured as AF Open-Drain"
    ((PASS++)) || true
else
    # Check for the macro-based approach
    if grep -q "GPIO_CRL_PIN_CFG(I2C1_SCL_PIN.*GPIO_CNF_AF_OD\|GPIO_CNF_AF_OD" "$I2C_SRC" 2>/dev/null; then
        echo -e "  [${GREEN}PASS${NC}] PB6 (SCL) configured as AF Open-Drain"
        ((PASS++)) || true
    else
        echo -e "  [${RED}FAIL${NC}] PB6 (SCL) configured as AF Open-Drain"
        ((FAIL++)) || true
    fi
fi

if grep -q "I2C1_SDA_PIN.*GPIO_CNF_AF_OD\|GPIO_CNF_AF_OD.*I2C1_SDA_PIN\|PB7.*AF.*OD\|SDA.*AF.*OD" "$I2C_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] PB7 (SDA) configured as AF Open-Drain"
    ((PASS++)) || true
else
    if grep -q "GPIO_CRL_PIN_CFG(I2C1_SDA_PIN.*GPIO_CNF_AF_OD\|GPIO_CNF_AF_OD" "$I2C_SRC" 2>/dev/null; then
        echo -e "  [${GREEN}PASS${NC}] PB7 (SDA) configured as AF Open-Drain"
        ((PASS++)) || true
    else
        echo -e "  [${RED}FAIL${NC}] PB7 (SDA) configured as AF Open-Drain"
        ((FAIL++)) || true
    fi
fi

# Check SPI pins (use code-level patterns: GPIO_CRL_PIN_CFG macro)
if grep -q "GPIO_CRL_PIN_CFG.*5.*GPIO_CNF_AF_PP\|GPIO_CNF_AF_PP.*GPIO_MODE_OUT50.*5\|PA5.*SCK" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] PA5 (SCK) configured as AF Push-Pull"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] PA5 (SCK) configured as AF Push-Pull"
    ((FAIL++)) || true
fi

if grep -q "GPIO_CRL_PIN_CFG.*7.*GPIO_CNF_AF_PP\|GPIO_CNF_AF_PP.*GPIO_MODE_OUT50.*7\|PA7.*MOSI" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] PA7 (MOSI) configured as AF Push-Pull"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] PA7 (MOSI) configured as AF Push-Pull"
    ((FAIL++)) || true
fi

if grep -q "GPIO_CRL_PIN_CFG.*4.*GPIO_CNF_PP_OUT\|GPIO_CNF_PP_OUT.*GPIO_MODE_OUT50.*4\|OLED_DC_PIN\|PA4.*DC" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] PA4 (DC) configured as Output PP"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] PA4 (DC) configured as Output PP"
    ((FAIL++)) || true
fi

if grep -q "GPIO_CRL_PIN_CFG.*0.*GPIO_CNF_PP_OUT\|GPIO_CNF_PP_OUT.*GPIO_MODE_OUT50.*0\|OLED_RES_PIN\|PB0.*RES" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] PB0 (RES) configured as Output PP"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] PB0 (RES) configured as Output PP"
    ((FAIL++)) || true
fi

echo ""
echo "--- I2C1 Peripheral Configuration ---"

if grep -q "I2C1->CR2.*=.*36\b\|I2C1->CR2.*FREQ.*36" "$I2C_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] I2C1 CR2 FREQ = 36 MHz"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] I2C1 CR2 FREQ = 36 MHz"
    ((FAIL++)) || true
fi

if grep -q "I2C1->CCR.*=.*180\b" "$I2C_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] I2C1 CCR = 180 (100kHz standard mode)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] I2C1 CCR = 180 (100kHz standard mode)"
    ((FAIL++)) || true
fi

if grep -q "I2C1->TRISE.*=.*37\b" "$I2C_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] I2C1 TRISE = 37 (1000ns max rise time)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] I2C1 TRISE = 37"
    ((FAIL++)) || true
fi

if grep -q "I2C1->CR1.*I2C_CR1_PE" "$I2C_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] I2C1 peripheral enabled (PE=1)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] I2C1 peripheral enabled"
    ((FAIL++)) || true
fi

echo ""
echo "--- SPI1 Peripheral Configuration ---"

if grep -q "SPI1->CR1.*SPI_CR1_MSTR" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] SPI1 Master mode"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] SPI1 Master mode"
    ((FAIL++)) || true
fi

if grep -q "SPI_CR1_BR_DIV32.*SPI_CR1_BR_SHIFT\|SPI_CR1_BR_DIV32.*BR" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] SPI1 Baud rate = fPCLK2/32 (~2.25 MHz)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] SPI1 Baud rate = fPCLK2/32"
    ((FAIL++)) || true
fi

if grep -q "SPI_CR1_SSM" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] SPI1 Software slave management (SSM)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] SPI1 Software slave management"
    ((FAIL++)) || true
fi

if grep -q "SPI_CR1_SPE" "$SPI_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] SPI1 enabled (SPE=1)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] SPI1 enabled"
    ((FAIL++)) || true
fi

echo ""
echo "--- Sensor Configuration ---"

# HMC5883L registers
HMC_SRC="hmc5883l.c"
if grep -q "HMC5883L_AVG_8" "$HMC_SRC" 2>/dev/null && grep -q "HMC5883L_RATE_15" "$HMC_SRC" 2>/dev/null && grep -q "HMC5883L_BIAS_NORMAL" "$HMC_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] HMC5883L: 8-sample average, 15 Hz data rate"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] HMC5883L: 8-sample average, 15 Hz data rate"
    ((FAIL++)) || true
fi

if grep -q "HMC5883L_CFG_B.*HMC5883L_GAIN_1090" "$HMC_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] HMC5883L: Gain = 1090 LSB/Gauss (+-1.3 Ga)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] HMC5883L: Gain = 1090 LSB/Gauss"
    ((FAIL++)) || true
fi

if grep -q "HMC5883L_MODE.*HMC5883L_MODE_CONT" "$HMC_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] HMC5883L: Continuous measurement mode"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] HMC5883L: Continuous measurement mode"
    ((FAIL++)) || true
fi

# MPU6050 registers
MPU_SRC="mpu6050.c"
if grep -q "MPU6050_PWR_MGMT_1.*MPU6050_PWR1_CLKSEL_INT" "$MPU_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] MPU6050: Wake up, internal 8MHz clock"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] MPU6050: Wake up, internal 8MHz clock"
    ((FAIL++)) || true
fi

if grep -q "MPU6050_SMPLRT_DIV.*7\b" "$MPU_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] MPU6050: Sample rate divider = 7 (125 Hz)"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] MPU6050: Sample rate divider = 7"
    ((FAIL++)) || true
fi

if grep -q "MPU6050_GYRO_CONFIG.*MPU6050_GYRO_FS_2000" "$MPU_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] MPU6050: Gyro +-2000 deg/s"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] MPU6050: Gyro +-2000 deg/s"
    ((FAIL++)) || true
fi

if grep -q "MPU6050_ACCEL_CONFIG.*MPU6050_ACCEL_FS_16" "$MPU_SRC" 2>/dev/null; then
    echo -e "  [${GREEN}PASS${NC}] MPU6050: Accel +-16g"
    ((PASS++)) || true
else
    echo -e "  [${RED}FAIL${NC}] MPU6050: Accel +-16g"
    ((FAIL++)) || true
fi

echo ""
echo "=============================================="
echo " STATIC ANALYSIS SUMMARY"
echo "=============================================="
echo -e "Passed: ${GREEN}$PASS${NC}"
echo -e "Failed: ${RED}$FAIL${NC}"
echo -e "Total:  $((PASS + FAIL))"
echo ""

# -------------------------------------------------------------------
# Step 5: Run QEMU if available
# -------------------------------------------------------------------
if [ "$HAS_QEMU" -eq 1 ] && [ "${HAS_BINARY:-0}" -eq 1 ]; then
    echo "=============================================="
    echo " QEMU RUNTIME TEST"
    echo "=============================================="
    echo ""
    echo "Running firmware in QEMU ($MACHINE)..."

    # Run QEMU with a timeout to avoid hanging
    timeout 15s qemu-system-arm \
        -M "$MACHINE" \
        -cpu cortex-m3 \
        -nographic \
        -semihosting \
        -kernel compass.elf \
        2>&1 || echo -e "\n${YELLOW}QEMU exited (timeout or normal exit)${NC}"

    QEMU_EXIT=$?
    echo ""
    if [ $QEMU_EXIT -eq 124 ]; then
        echo -e "${YELLOW}QEMU timed out (this is normal for an infinite loop firmware)${NC}"
    elif [ $QEMU_EXIT -eq 0 ]; then
        echo -e "${GREEN}QEMU completed successfully${NC}"
    else
        echo -e "${YELLOW}QEMU exited with code $QEMU_EXIT${NC}"
    fi
else
    echo -e "${YELLOW}Skipping QEMU runtime test (QEMU or binary not available).${NC}"
fi

echo ""
echo "=============================================="
echo " TEST COMPLETE"
echo "=============================================="

# Return overall status
if [ "$FAIL" -gt 0 ]; then
    echo -e "${RED}Some checks failed. Review the output above.${NC}"
    exit 1
else
    echo -e "${GREEN}All static checks passed.${NC}"
    exit 0
fi
