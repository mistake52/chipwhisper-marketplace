# MPU6050 Driver for STM32F103C8T6

Bare-metal register-level firmware (no HAL) that reads accelerometer and gyroscope data from an MPU6050 module via I2C and prints the results to USART1 at 1Hz.

## Pin Assignment

| STM32 Pin | Function         | Connected To  | GPIO Config              |
|-----------|------------------|---------------|--------------------------|
| PB6       | I2C1_SCL         | MPU6050 SCL   | AF open-drain, 50MHz     |
| PB7       | I2C1_SDA         | MPU6050 SDA   | AF open-drain, 50MHz     |
| PA9       | USART1_TX        | Serial RX     | AF push-pull, 50MHz      |
| 3.3V      | Power            | MPU6050 VCC   | —                        |
| GND       | Ground           | MPU6050 GND   | —                        |

## Wiring Diagram

```
STM32F103C8T6          MPU6050
+-----------+         +---------+
|      PB6  |---------| SCL     |
|      PB7  |---------| SDA     |
|      3.3V |---------| VCC     |
|      GND  |---------| GND     |
|      PA9  |----> Serial adapter RX
+-----------+         +---------+
```

Note: External pull-up resistors (4.7kΩ) on SCL/SDA are recommended. Most MPU6050 modules include them on-board.

## Build

```bash
make clean && make all
```

Requires `arm-none-eabi-gcc` toolchain installed.

## Flash

Via ST-Link:
```bash
st-flash write mpu6050_demo.bin 0x08000000
```

Via DAPlink: copy `mpu6050_demo.bin` to the virtual USB drive.

## Serial Output

Baud rate: 115200, 8N1. Expected output every 1 second:

```
MPU6050 Driver Ready
Accel: 0.01, -0.03, 1.02  |  Gyro: 0.12, -0.05, 0.31
```

## Register Values (MCP-verified, RM0008 Rev 21)

- Clock: HSE 8MHz → PLL x9 → 72MHz, APB1/2, APB2/1
- I2C1: 100kHz standard mode, FREQ=36, CCR=180, TRISE=37
- USART1: 115200 baud, BRR=625 (72000000/115200)
