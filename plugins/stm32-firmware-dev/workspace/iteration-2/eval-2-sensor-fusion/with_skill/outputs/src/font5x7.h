/**
 * font5x7.h — 5×7 pixel font for SSD1306 OLED
 * ASCII 32 (space) through 126 (~), 95 glyphs.
 */

#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

#define FONT5X7_WIDTH   5
#define FONT5X7_HEIGHT  7
#define FONT5X7_START   32
#define FONT5X7_COUNT   95

extern const uint8_t font5x7[FONT5X7_COUNT][FONT5X7_WIDTH];

#endif /* FONT5X7_H */
