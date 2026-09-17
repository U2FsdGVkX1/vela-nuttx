/****************************************************************************
 * arch/arm64/src/s6/s6_gpio.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_S6_S6_GPIO_H
#define __ARCH_ARM64_SRC_S6_S6_GPIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Global GPIO numbering used by the GPIO interrupt controller. */

#define S6_GPIOA_NPINS           16
#define S6_GPIOA(n)               (82u + (n))

/* GPIO interrupt channels 0..11 are connected to GIC SPIs 10..21. */

#define S6_GPIOINT_NCHANNELS      12
#define S6_GPIOINT_IRQ(channel)    (42 + (channel))

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/* GPIOA bank (pins 0-15) */

EXTERN int s6_gpioa_set_mux(uint8_t pin, uint8_t func);
EXTERN int s6_gpioa_config_input(uint8_t pin);
EXTERN int s6_gpioa_config_output(uint8_t pin, bool value);
EXTERN void s6_gpioa_write(uint8_t pin, bool value);

/* GPIO interrupt controller (channels 0-11, GIC INTID 42 + channel) */

EXTERN int s6_gpioint_config(uint8_t channel, uint8_t gpio, int trigger);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM64_SRC_S6_S6_GPIO_H */
