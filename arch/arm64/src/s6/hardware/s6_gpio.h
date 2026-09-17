/****************************************************************************
 * arch/arm64/src/s6/hardware/s6_gpio.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_S6_HARDWARE_S6_GPIO_H
#define __ARCH_ARM64_SRC_S6_HARDWARE_S6_GPIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <arch/chip/chip.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Periphs pinctrl / GPIO interrupt controller base addresses */

#define S6_GPIO_MUX_BASE          (CONFIG_DEVICEIO_BASEADDR + 0x4000)
#define S6_GPIO_INTC_BASE         (CONFIG_DEVICEIO_BASEADDR + 0x4080)
#define S6_GPIO_GPIO_BASE         (CONFIG_DEVICEIO_BASEADDR + 0x40c0)

/* GPIOA bank registers.  The pinctrl driver stores register indices that
 * are word-scaled on access ((index + bit / 32) * 4), so the offsets below
 * are the resulting physical offsets from S6_GPIO_GPIO_BASE:
 *   OUT = 0x071 * 4 = 0x1c4
 *   DIR = 0x072 * 4 = 0x1c8 (EN_N semantics: bit set is input)
 */

#define S6_GPIOA_OUT_OFFSET       0x1c4
#define S6_GPIOA_DIR_OFFSET       0x1c8

/* GPIOA pin mux: bank index 0x010, word-scaled on access (x4), four bits
 * per pin (16 pins span two words, 8 pins each).
 */

#define S6_GPIOA_MUX_OFFSET(pin)  (0x040 + ((pin) >> 3) * 4)
#define S6_GPIOA_MUX_SHIFT(pin)   (((pin) & 7) * 4)

/* GPIO interrupt controller (12 channels mapped to GIC SPI 10..21, i.e.
 * GIC INTIDs 42..53).  Channel selection register layout:
 *   PIN_SEL(channel)   select register for the GPIO number
 *   EDGE_POL           bit(0+n): polarity low, bit(12+n): edge,
 *                      bit31: global enable switch
 *   EDGE_POL_EXTR      bit(n): both-edges mode
 */

#define S6_GPIOINT_EDGE_POL_OFFSET      0x00
#define S6_GPIOINT_PIN_SEL_OFFSET(ch)   (0x04 + ((ch) >> 1) * 4)
#define S6_GPIOINT_EDGE_POL_EXTR_OFFSET 0x1c

#define S6_GPIOINT_GLOBAL_EN      (1u << 31)
#define S6_GPIOINT_POL_LOW(ch)    (1u << (ch))
#define S6_GPIOINT_EDGE(ch)       (1u << (12 + (ch)))
#define S6_GPIOINT_BOTH(ch)       (1u << (ch))

#define S6_GPIOINT_PIN_SEL_SHIFT(ch)  (((ch) & 1) * 16)
#define S6_GPIOINT_PIN_SEL_MASK(ch)   (0xffu << S6_GPIOINT_PIN_SEL_SHIFT(ch))

#endif /* __ARCH_ARM64_SRC_S6_HARDWARE_S6_GPIO_H */
