/****************************************************************************
 * arch/arm64/src/s6/s6_gpio.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/* Minimal GPIO support for the Amlogic S6 periphs GPIOA bank and the GPIO
 * interrupt controller (SC2-style).  Register layout follows the vendor
 * pinctrl / gpio-intc drivers.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>

#include <nuttx/arch.h>

#include "arm64_internal.h"
#include "s6_gpio.h"
#include "hardware/s6_gpio.h"

#ifdef CONFIG_S6_GPIO

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: s6_gpioa_set_mux
 *
 * Description:
 *   Select the peripheral function of a GPIOA pin (0-15); function 0 is
 *   the plain GPIO function.
 *
 ****************************************************************************/

int s6_gpioa_set_mux(uint8_t pin, uint8_t func)
{
  if (pin >= S6_GPIOA_NPINS)
    {
      return -EINVAL;
    }

  modifyreg32(S6_GPIO_MUX_BASE + S6_GPIOA_MUX_OFFSET(pin),
              0xfu << S6_GPIOA_MUX_SHIFT(pin),
              (func & 0xfu) << S6_GPIOA_MUX_SHIFT(pin));
  return OK;
}

/****************************************************************************
 * Name: s6_gpioa_write
 *
 * Description:
 *   Set the output level of a GPIOA pin (0-15).
 *
 ****************************************************************************/

void s6_gpioa_write(uint8_t pin, bool value)
{
  DEBUGASSERT(pin < S6_GPIOA_NPINS);

  modifyreg32(S6_GPIO_GPIO_BASE + S6_GPIOA_OUT_OFFSET,
              1u << pin, value ? (1u << pin) : 0);
}

/****************************************************************************
 * Name: s6_gpioa_config_input
 *
 * Description:
 *   Configure a GPIOA pin (0-15) as a plain GPIO input.
 *
 ****************************************************************************/

int s6_gpioa_config_input(uint8_t pin)
{
  int ret;

  ret = s6_gpioa_set_mux(pin, 0);
  if (ret < 0)
    {
      return ret;
    }

  modifyreg32(S6_GPIO_GPIO_BASE + S6_GPIOA_DIR_OFFSET,
              1u << pin, 1u << pin);
  return OK;
}

/****************************************************************************
 * Name: s6_gpioa_config_output
 *
 * Description:
 *   Configure a GPIOA pin (0-15) as a plain GPIO output with an initial
 *   level.
 *
 ****************************************************************************/

int s6_gpioa_config_output(uint8_t pin, bool value)
{
  int ret;

  ret = s6_gpioa_set_mux(pin, 0);
  if (ret < 0)
    {
      return ret;
    }

  s6_gpioa_write(pin, value);
  modifyreg32(S6_GPIO_GPIO_BASE + S6_GPIOA_DIR_OFFSET, 1u << pin, 0);
  return OK;
}

/****************************************************************************
 * Name: s6_gpioint_config
 *
 * Description:
 *   Route a GPIO (global number, e.g. S6_GPIOA(10)) to a GPIO interrupt
 *   controller channel (0-11) and program its trigger type.  The interrupt
 *   then arrives on GIC INTID 42 + channel; masking happens at the GIC.
 *
 * Input Parameters:
 *   channel - GPIO interrupt controller channel (0-11)
 *   gpio    - Global GPIO number
 *   trigger - One of the IRQ_* trigger modes from nuttx/arch.h
 *
 ****************************************************************************/

int s6_gpioint_config(uint8_t channel, uint8_t gpio, int trigger)
{
  uint32_t reg;
  uint32_t val = 0;

  if (channel >= S6_GPIOINT_NCHANNELS)
    {
      return -EINVAL;
    }

  /* Global interrupt switch (harmless to re-set) */

  modifyreg32(S6_GPIO_INTC_BASE + S6_GPIOINT_EDGE_POL_OFFSET,
              0, S6_GPIOINT_GLOBAL_EN);

  /* Bind the GPIO number to the channel */

  reg = S6_GPIO_INTC_BASE + S6_GPIOINT_PIN_SEL_OFFSET(channel);
  modifyreg32(reg, S6_GPIOINT_PIN_SEL_MASK(channel),
              (gpio << S6_GPIOINT_PIN_SEL_SHIFT(channel)) &
              S6_GPIOINT_PIN_SEL_MASK(channel));

  /* Configure the trigger type. */

  modifyreg32(S6_GPIO_INTC_BASE + S6_GPIOINT_EDGE_POL_EXTR_OFFSET,
              S6_GPIOINT_BOTH(channel),
              trigger == IRQ_BOTH_EDGE ? S6_GPIOINT_BOTH(channel) : 0);

  if (trigger == IRQ_FALLING_EDGE || trigger == IRQ_LOW_LEVEL)
    {
      val |= S6_GPIOINT_POL_LOW(channel);
    }

  if (trigger == IRQ_RISING_EDGE || trigger == IRQ_FALLING_EDGE)
    {
      val |= S6_GPIOINT_EDGE(channel);
    }

  modifyreg32(S6_GPIO_INTC_BASE + S6_GPIOINT_EDGE_POL_OFFSET,
              S6_GPIOINT_POL_LOW(channel) | S6_GPIOINT_EDGE(channel), val);
  return OK;
}

#endif /* CONFIG_S6_GPIO */
