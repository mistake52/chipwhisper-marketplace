/**
 * rcc.h — System clock configuration
 *
 * MCP-Verified (RM0008 Rev 21, P90-P125):
 *   RCC_BASE   = 0x40021000
 *   FLASH_BASE = 0x40022000
 */

#ifndef RCC_H
#define RCC_H

#include <stdint.h>

void rcc_init(void);

#endif /* RCC_H */
