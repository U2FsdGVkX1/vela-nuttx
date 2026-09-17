/****************************************************************************
 * arch/arm64/src/s6/s6_serial.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/* Amlogic Meson S6 UART-B lower-half. */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/serial/serial.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "arm64_internal.h"
#include "hardware/s6_uart.h"

#ifdef USE_SERIALDRIVER

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct s6_uart_priv_s
{
  uintptr_t base;
  int       irq;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  s6_uart_setup(struct uart_dev_s *dev);
static void s6_uart_shutdown(struct uart_dev_s *dev);
static int  s6_uart_attach(struct uart_dev_s *dev);
static void s6_uart_detach(struct uart_dev_s *dev);
static int  s6_uart_ioctl(struct file *filep, int cmd, unsigned long arg);
static int  s6_uart_receive(struct uart_dev_s *dev, unsigned int *status);
static void s6_uart_rxint(struct uart_dev_s *dev, bool enable);
static bool s6_uart_rxavailable(struct uart_dev_s *dev);
static void s6_uart_send(struct uart_dev_s *dev, int ch);
static void s6_uart_txint(struct uart_dev_s *dev, bool enable);
static bool s6_uart_txready(struct uart_dev_s *dev);
static bool s6_uart_txempty(struct uart_dev_s *dev);
static int  s6_uart_interrupt(int irq, void *context, void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_ops_s g_s6_uart_ops =
{
  .setup        = s6_uart_setup,
  .shutdown     = s6_uart_shutdown,
  .attach       = s6_uart_attach,
  .detach       = s6_uart_detach,
  .ioctl        = s6_uart_ioctl,
  .receive      = s6_uart_receive,
  .rxint        = s6_uart_rxint,
  .rxavailable  = s6_uart_rxavailable,
  .send         = s6_uart_send,
  .txint        = s6_uart_txint,
  .txready      = s6_uart_txready,
  .txempty      = s6_uart_txempty,
};

static struct s6_uart_priv_s g_uart0priv =
{
  .base = CONFIG_S6_UART0_BASE,
  .irq  = CONFIG_S6_UART0_IRQ,
};

static char g_uart0rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_uart0txbuffer[CONFIG_UART0_TXBUFSIZE];

static struct uart_dev_s g_uart0port =
{
  .isconsole = true,
  .recv      =
    {
      .size   = CONFIG_UART0_RXBUFSIZE,
      .buffer = g_uart0rxbuffer,
    },
  .xmit      =
    {
      .size   = CONFIG_UART0_TXBUFSIZE,
      .buffer = g_uart0txbuffer,
    },
  .ops       = &g_s6_uart_ops,
  .priv      = &g_uart0priv,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t s6_uart_getreg(const struct s6_uart_priv_s *priv,
                                      unsigned int offset)
{
  return getreg32(priv->base + offset);
}

static inline void s6_uart_putreg(const struct s6_uart_priv_s *priv,
                                  unsigned int offset, uint32_t value)
{
  putreg32(value, priv->base + offset);
}

static inline uint32_t s6_uart_data_bits(void)
{
  switch (CONFIG_UART0_BITS)
    {
      case 5:
        return S6_UART_CONTROL_DATA_LEN_5BIT;
      case 6:
        return S6_UART_CONTROL_DATA_LEN_6BIT;
      case 7:
        return S6_UART_CONTROL_DATA_LEN_7BIT;
      default:
        return S6_UART_CONTROL_DATA_LEN_8BIT;
    }
}

static int s6_uart_setup(struct uart_dev_s *dev)
{
  const struct s6_uart_priv_s *priv = dev->priv;
  uint32_t control;

  control = s6_uart_getreg(priv, S6_UART_CONTROL_OFFSET);
  control |= S6_UART_CONTROL_TX_RST | S6_UART_CONTROL_RX_RST |
             S6_UART_CONTROL_CLR_ERR;
  s6_uart_putreg(priv, S6_UART_CONTROL_OFFSET, control);

  control &= ~(S6_UART_CONTROL_TX_RST | S6_UART_CONTROL_RX_RST |
               S6_UART_CONTROL_CLR_ERR | S6_UART_CONTROL_DATA_LEN_MASK |
               S6_UART_CONTROL_STOP_2 | S6_UART_CONTROL_PARITY_EN |
               S6_UART_CONTROL_PARITY_ODD | S6_UART_CONTROL_RX_INT_EN |
               S6_UART_CONTROL_TX_INT_EN);
  control |= S6_UART_CONTROL_TWO_WIRE | s6_uart_data_bits();

#if CONFIG_UART0_2STOP
  control |= S6_UART_CONTROL_STOP_2;
#endif

#if CONFIG_UART0_PARITY == 1
  control |= S6_UART_CONTROL_PARITY_EN | S6_UART_CONTROL_PARITY_ODD;
#elif CONFIG_UART0_PARITY == 2
  control |= S6_UART_CONTROL_PARITY_EN;
#endif

  s6_uart_putreg(priv, S6_UART_REG5_OFFSET,
                 S6_UART_REG5_USE_NEW_BAUD | S6_UART_REG5_USE_XTAL |
                 S6_UART_REG5_XTAL_DIV2 |
                 (S6_UART_BAUD_DIVISOR(CONFIG_UART0_BAUD) &
                  S6_UART_REG5_BAUD_MASK));
  s6_uart_putreg(priv, S6_UART_MISC_OFFSET,
                 S6_UART_MISC_TX_LEVEL(32) | S6_UART_MISC_RX_LEVEL(1));

  control |= S6_UART_CONTROL_TX_EN | S6_UART_CONTROL_RX_EN;
  s6_uart_putreg(priv, S6_UART_CONTROL_OFFSET, control);
  return OK;
}

static void s6_uart_shutdown(struct uart_dev_s *dev)
{
  const struct s6_uart_priv_s *priv = dev->priv;
  uint32_t control = s6_uart_getreg(priv, S6_UART_CONTROL_OFFSET);

  control &= ~(S6_UART_CONTROL_TX_EN | S6_UART_CONTROL_RX_EN |
               S6_UART_CONTROL_RX_INT_EN | S6_UART_CONTROL_TX_INT_EN);
  s6_uart_putreg(priv, S6_UART_CONTROL_OFFSET, control);
}

static int s6_uart_attach(struct uart_dev_s *dev)
{
  const struct s6_uart_priv_s *priv = dev->priv;
  int ret = irq_attach(priv->irq, s6_uart_interrupt, dev);

  if (ret == OK)
    {
      /* UART-B is edge-triggered on S6, unlike the level-triggered
       * consoles on most other SoCs.
       */

      up_prioritize_irq(priv->irq, 0);
      up_set_irq_type(priv->irq, IRQ_RISING_EDGE);
      up_enable_irq(priv->irq);
    }

  return ret;
}

static void s6_uart_detach(struct uart_dev_s *dev)
{
  const struct s6_uart_priv_s *priv = dev->priv;

  up_disable_irq(priv->irq);
  irq_detach(priv->irq);
}

static int s6_uart_ioctl(struct file *filep, int cmd, unsigned long arg)
{
  UNUSED(filep);
  UNUSED(cmd);
  UNUSED(arg);
  return -ENOTTY;
}

static int s6_uart_receive(struct uart_dev_s *dev, unsigned int *status)
{
  const struct s6_uart_priv_s *priv = dev->priv;
  uint32_t uart_status = s6_uart_getreg(priv, S6_UART_STATUS_OFFSET);

  *status = uart_status;
  if (uart_status & (S6_UART_STATUS_PARITY_ERR |
                     S6_UART_STATUS_FRAME_ERR |
                     S6_UART_STATUS_RX_OVERFLOW))
    {
      uint32_t control = s6_uart_getreg(priv, S6_UART_CONTROL_OFFSET);

      s6_uart_putreg(priv, S6_UART_CONTROL_OFFSET,
                     control | S6_UART_CONTROL_CLR_ERR);
      s6_uart_putreg(priv, S6_UART_CONTROL_OFFSET, control);
    }

  return s6_uart_getreg(priv, S6_UART_RFIFO_OFFSET) & 0xff;
}

static void s6_uart_rxint(struct uart_dev_s *dev, bool enable)
{
  const struct s6_uart_priv_s *priv = dev->priv;

  modifyreg32(priv->base + S6_UART_CONTROL_OFFSET,
              S6_UART_CONTROL_RX_INT_EN,
              enable ? S6_UART_CONTROL_RX_INT_EN : 0);
}

static bool s6_uart_rxavailable(struct uart_dev_s *dev)
{
  const struct s6_uart_priv_s *priv = dev->priv;

  return (s6_uart_getreg(priv, S6_UART_STATUS_OFFSET) &
          S6_UART_STATUS_RX_EMPTY) == 0;
}

static void s6_uart_send(struct uart_dev_s *dev, int ch)
{
  const struct s6_uart_priv_s *priv = dev->priv;

  s6_uart_putreg(priv, S6_UART_WFIFO_OFFSET, (uint32_t)ch);
}

static void s6_uart_txint(struct uart_dev_s *dev, bool enable)
{
  const struct s6_uart_priv_s *priv = dev->priv;

  modifyreg32(priv->base + S6_UART_CONTROL_OFFSET,
              S6_UART_CONTROL_TX_INT_EN,
              enable ? S6_UART_CONTROL_TX_INT_EN : 0);

  /* Enabling TINT does not necessarily create an edge when the FIFO was
   * already empty.  Kick the upper-half transmitter once so the first
   * buffered byte is placed in WFIFO; subsequent bytes are drained by IRQ.
   */

  if (enable)
    {
      uart_xmitchars(dev);
    }
}

static bool s6_uart_txready(struct uart_dev_s *dev)
{
  const struct s6_uart_priv_s *priv = dev->priv;

  return (s6_uart_getreg(priv, S6_UART_STATUS_OFFSET) &
          S6_UART_STATUS_TX_FULL) == 0;
}

static bool s6_uart_txempty(struct uart_dev_s *dev)
{
  const struct s6_uart_priv_s *priv = dev->priv;

  return (s6_uart_getreg(priv, S6_UART_STATUS_OFFSET) &
          S6_UART_STATUS_TX_EMPTY) != 0;
}

static int s6_uart_interrupt(int irq, void *context, void *arg)
{
  struct uart_dev_s *dev = arg;

  UNUSED(irq);
  UNUSED(context);

  if (s6_uart_rxavailable(dev))
    {
      uart_recvchars(dev);
    }

  if (s6_uart_txready(dev))
    {
      uart_xmitchars(dev);
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void arm64_earlyserialinit(void)
{
  uint32_t control;

  /* Preserve the UART clock, baud, FIFO and flow-control state installed by
   * U-Boot.  The S6 warm-reset path does not reliably restore every UART
   * register, so rewriting them here can break the next U-Boot session.
   * Clear only the interrupt enables: an IRQ can already be pending when
   * NuttX initializes the GIC, before the serial upper half has attached its
   * handler.
   */

  control = s6_uart_getreg(&g_uart0priv, S6_UART_CONTROL_OFFSET);
  control &= ~(S6_UART_CONTROL_RX_INT_EN | S6_UART_CONTROL_TX_INT_EN);
  control |= S6_UART_CONTROL_TX_EN | S6_UART_CONTROL_RX_EN;
  s6_uart_putreg(&g_uart0priv, S6_UART_CONTROL_OFFSET, control);

  /* The console may be registered without calling the lower-half setup
   * method.  Make the FIFO interrupt thresholds deterministic instead of
   * inheriting an arbitrary value left by U-Boot.
   */

  s6_uart_putreg(&g_uart0priv, S6_UART_MISC_OFFSET,
                 S6_UART_MISC_TX_LEVEL(32) | S6_UART_MISC_RX_LEVEL(1));
}

void arm64_serialinit(void)
{
  (void)uart_register("/dev/console", &g_uart0port);
  (void)uart_register("/dev/ttyS0", &g_uart0port);
}

void up_putc(int ch)
{
  arm64_lowputc((char)ch);
}

#endif /* USE_SERIALDRIVER */
