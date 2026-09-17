/****************************************************************************
 * arch/arm64/src/s6/s6_i2c.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <arch/chip/chip.h>
#include <debug.h>
#include <nuttx/arch.h>
#include <nuttx/mutex.h>

#include "arm64_internal.h"
#include "s6_i2c.h"
#include "hardware/s6_i2c.h"

#ifdef CONFIG_S6_I2C

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Timeout for a single token chunk */

#define S6_I2C_TIMEOUT_MS         500
#define S6_I2C_POLL_DELAY_US      50
#define S6_I2C_POLL_COUNT \
  (S6_I2C_TIMEOUT_MS * 1000 / S6_I2C_POLL_DELAY_US)

/* Data bytes per token chunk (RDATA/WDATA registers hold 8 bytes) */

#define S6_I2C_CHUNK_SIZE         8

#define DIV_ROUND_UP(n, d)        (((n) + (d) - 1) / (d))

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* I2C device private data */

struct s6_i2c_dev_s
{
  struct i2c_master_s dev;    /* I2C master device (must be first) */
  uintptr_t base;             /* Controller base address */
  uint32_t frequency;         /* Configured bus frequency */
  mutex_t  lock;              /* Serialize transfers on the bus */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int s6_i2c_transfer(struct i2c_master_s *dev,
                           struct i2c_msg_s *msgs, int count);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct i2c_ops_s g_s6_i2c_ops =
{
  .transfer = s6_i2c_transfer
};

static struct s6_i2c_dev_s g_s6_i2c0 =
{
  .dev       =
    {
      .ops = &g_s6_i2c_ops
    },
  .base      = CONFIG_S6_I2C0_BASE,
  .frequency = 0,
  .lock      = NXMUTEX_INITIALIZER
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: s6_i2c_set_clk_div
 *
 * Description:
 *   Install the SCL high/low dividers for the requested bus frequency,
 *   following the vendor (AMLOGIC_MODIFY) divider algorithm.
 *
 ****************************************************************************/

static void s6_i2c_set_clk_div(FAR struct s6_i2c_dev_s *priv,
                               uint32_t frequency)
{
  uint32_t div_temp;
  uint32_t div_h;
  uint32_t div_l;

  if (frequency >= 400000)
    {
      /* Fast mode: H/L duty 2/3 */

      div_temp = DIV_ROUND_UP(CONFIG_S6_I2C_CLK_HZ * 2, frequency * 5);
      div_h    = div_temp - 15;
      div_l    = DIV_ROUND_UP(CONFIG_S6_I2C_CLK_HZ * 3, frequency * 10);
    }
  else
    {
      /* Standard mode: 50% duty */

      div_temp = DIV_ROUND_UP(CONFIG_S6_I2C_CLK_HZ, frequency);
      div_h    = DIV_ROUND_UP(div_temp, 2) - 15;
      div_l    = DIV_ROUND_UP(div_temp, 4);
    }

  /* The divider fields are 12 bits wide */

  if (div_h >= (1 << 12))
    {
      div_h = (1 << 12) - 1;
    }

  if (div_l >= (1 << 12))
    {
      div_l = (1 << 12) - 1;
    }

  iinfo("freq %u, div_h %u, div_l %u\n", frequency, div_h, div_l);

  modifyreg32(priv->base + S6_I2C_CTRL_OFFSET,
              S6_I2C_CTRL_CLKDIV_MASK,
              (div_h & 0x3ff) << S6_I2C_CTRL_CLKDIV_SHIFT);
  modifyreg32(priv->base + S6_I2C_CTRL_OFFSET,
              S6_I2C_CTRL_CLKDIVEXT_MASK,
              (div_h >> 10) << S6_I2C_CTRL_CLKDIVEXT_SHIFT);
  modifyreg32(priv->base + S6_I2C_SLAVE_ADDR_OFFSET,
              S6_I2C_SLV_DIVL_MASK,
              (div_l << S6_I2C_SLV_DIVL_SHIFT) & S6_I2C_SLV_DIVL_MASK);
  modifyreg32(priv->base + S6_I2C_SLAVE_ADDR_OFFSET,
              S6_I2C_SLV_DIVL_EN, S6_I2C_SLV_DIVL_EN);
}

/****************************************************************************
 * Name: s6_i2c_add_token
 *
 * Description:
 *   Append a token command to the packed token list (up to 8 per register).
 *
 ****************************************************************************/

static void s6_i2c_add_token(FAR uint64_t *tokens, FAR int *ntok, int token)
{
  *tokens |= (uint64_t)token << (*ntok * 4);
  (*ntok)++;
}

/****************************************************************************
 * Name: s6_i2c_put_data
 *
 * Description:
 *   Pack up to eight bytes into the controller's write data registers.
 *
 ****************************************************************************/

static void s6_i2c_put_data(FAR struct s6_i2c_dev_s *priv,
                            FAR const uint8_t *buffer, int length)
{
  uint32_t data[2] =
  {
    0, 0
  };

  int i;

  for (i = 0; i < length; i++)
    {
      data[i >> 2] |= (uint32_t)buffer[i] << ((i & 3) * 8);
    }

  putreg32(data[0], priv->base + S6_I2C_TOK_WDATA0_OFFSET);
  putreg32(data[1], priv->base + S6_I2C_TOK_WDATA1_OFFSET);
}

/****************************************************************************
 * Name: s6_i2c_get_data
 *
 * Description:
 *   Unpack up to eight bytes from the controller's read data registers.
 *
 ****************************************************************************/

static void s6_i2c_get_data(FAR struct s6_i2c_dev_s *priv,
                            FAR uint8_t *buffer, int length)
{
  uint32_t data[2];
  int i;

  data[0] = getreg32(priv->base + S6_I2C_TOK_RDATA0_OFFSET);
  data[1] = getreg32(priv->base + S6_I2C_TOK_RDATA1_OFFSET);

  for (i = 0; i < length; i++)
    {
      buffer[i] = (data[i >> 2] >> ((i & 3) * 8)) & 0xff;
    }
}

/****************************************************************************
 * Name: s6_i2c_run_tokens
 *
 * Description:
 *   Write the token list, run it on the 0 -> 1 edge of START and poll until
 *   the controller finishes.  Returns -ENXIO on a NAK and -ETIMEDOUT when a
 *   chunk does not complete in time (aborting the controller).
 *
 ****************************************************************************/

static int s6_i2c_run_tokens(FAR struct s6_i2c_dev_s *priv, uint64_t tokens)
{
  uint32_t timeout = S6_I2C_POLL_COUNT;
  uint32_t ctrl;

  putreg32((uint32_t)tokens, priv->base + S6_I2C_TOK_LIST0_OFFSET);
  putreg32((uint32_t)(tokens >> 32), priv->base + S6_I2C_TOK_LIST1_OFFSET);

  /* A chunk runs on every 0 -> 1 transition of START */

  modifyreg32(priv->base + S6_I2C_CTRL_OFFSET, S6_I2C_CTRL_START, 0);
  modifyreg32(priv->base + S6_I2C_CTRL_OFFSET,
              S6_I2C_CTRL_START, S6_I2C_CTRL_START);

  for (;;)
    {
      ctrl = getreg32(priv->base + S6_I2C_CTRL_OFFSET);
      if ((ctrl & S6_I2C_CTRL_STATUS) == 0)
        {
          /* The controller is idle; the same read tells a NAK from a
           * clean completion.
           */

          if ((ctrl & S6_I2C_CTRL_ERROR) != 0)
            {
              ierr("NAK\n");
              return -ENXIO;
            }

          return OK;
        }

      if (--timeout == 0)
        {
          /* Abort anything left running */

          modifyreg32(priv->base + S6_I2C_CTRL_OFFSET,
                      S6_I2C_CTRL_START, 0);
          ierr("transfer timeout\n");
          return -ETIMEDOUT;
        }

      up_udelay(S6_I2C_POLL_DELAY_US);
    }
}

/****************************************************************************
 * Name: s6_i2c_xfer_msg
 *
 * Description:
 *   Transfer one message in token chunks of at most 8 data bytes.  When
 *   'cont' is set the transfer continues in a NOSTART follow-up message,
 *   so no STOP is issued and a read is not NAKed yet.
 *
 ****************************************************************************/

static int s6_i2c_xfer_msg(FAR struct s6_i2c_dev_s *priv,
                           FAR struct i2c_msg_s *msg, bool cont)
{
  bool write = (msg->flags & I2C_M_READ) == 0;
  bool stop = !cont && (msg->flags & I2C_M_NOSTOP) == 0;
  int len = msg->length;
  int pos = 0;

  iinfo("addr %02x len %d flags %04x\n", msg->addr, len, msg->flags);

  /* START and the slave address only appear in the first chunk */

  if ((msg->flags & I2C_M_NOSTART) == 0)
    {
      modifyreg32(priv->base + S6_I2C_SLAVE_ADDR_OFFSET,
                  S6_I2C_SLV_ADDR_MASK,
                  (msg->addr << 1) & S6_I2C_SLV_ADDR_MASK);
    }

  /* Reconfigure the dividers only when the frequency changes */

  if (msg->frequency != 0 && msg->frequency != priv->frequency)
    {
      s6_i2c_set_clk_div(priv, msg->frequency);
      priv->frequency = msg->frequency;
    }

  do
    {
      uint64_t tokens = 0;
      int ntok = 0;
      int count = len - pos;
      int i;
      int ret;

      if (count > S6_I2C_CHUNK_SIZE)
        {
          count = S6_I2C_CHUNK_SIZE;
        }

      if (pos == 0 && (msg->flags & I2C_M_NOSTART) == 0)
        {
          s6_i2c_add_token(&tokens, &ntok, S6_I2C_TOKEN_START);
          s6_i2c_add_token(&tokens, &ntok, write ?
                           S6_I2C_TOKEN_SLAVE_ADDR_WRITE :
                           S6_I2C_TOKEN_SLAVE_ADDR_READ);
        }

      for (i = 0; i < count - 1; i++)
        {
          s6_i2c_add_token(&tokens, &ntok, S6_I2C_TOKEN_DATA);
        }

      if (count > 0)
        {
          /* The final byte of a read message must be NAKed, unless more
           * data follows in this message or a NOSTART follow-up message.
           */

          if (write || pos + count < len || cont)
            {
              s6_i2c_add_token(&tokens, &ntok, S6_I2C_TOKEN_DATA);
            }
          else
            {
              s6_i2c_add_token(&tokens, &ntok, S6_I2C_TOKEN_DATA_LAST);
            }
        }

      if (stop && pos + count >= len)
        {
          s6_i2c_add_token(&tokens, &ntok, S6_I2C_TOKEN_STOP);
        }

      if (write && count > 0)
        {
          s6_i2c_put_data(priv, &msg->buffer[pos], count);
        }

      ret = s6_i2c_run_tokens(priv, tokens);
      if (ret < 0)
        {
          return ret;
        }

      if (!write)
        {
          s6_i2c_get_data(priv, &msg->buffer[pos], count);
        }

      pos += count;
    }
  while (pos < len);

  return OK;
}

/****************************************************************************
 * Name: s6_i2c_transfer
 *
 * Description:
 *   Validate and perform a sequence of I2C messages on the bus.
 *
 ****************************************************************************/

static int s6_i2c_transfer(struct i2c_master_s *dev,
                           FAR struct i2c_msg_s *msgs, int count)
{
  FAR struct s6_i2c_dev_s *priv = (FAR struct s6_i2c_dev_s *)dev;
  uint32_t ctrl;
  int ret = OK;
  int i;

  DEBUGASSERT(priv != NULL && msgs != NULL && count > 0);

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  /* Both lines held low means the bus is not usable (no pull-ups) */

  ctrl = getreg32(priv->base + S6_I2C_CTRL_OFFSET);
  if ((ctrl & (S6_I2C_CTRL_SCL_LEVEL | S6_I2C_CTRL_SDA_LEVEL)) == 0)
    {
      ierr("SCL and SDA are low, no pull-up?\n");
      nxmutex_unlock(&priv->lock);
      return -ENXIO;
    }

  for (i = 0; i < count; i++)
    {
      /* A NOSTART message continues the previous one, so it must follow a
       * message to the same slave in the same direction.
       */

      if ((msgs[i].flags & I2C_M_NOSTART) != 0 &&
          (i == 0 || msgs[i].addr != msgs[i - 1].addr ||
           ((msgs[i].flags ^ msgs[i - 1].flags) & I2C_M_READ) != 0))
        {
          ret = -EINVAL;
          break;
        }

      if ((msgs[i].flags & I2C_M_TEN) != 0)
        {
          ret = -ENOTSUP;
          break;
        }

      if (msgs[i].addr > 0x7f || msgs[i].length < 0 ||
          (msgs[i].length > 0 && msgs[i].buffer == NULL) ||
          msgs[i].frequency > I2C_SPEED_HIGH)
        {
          ret = -EINVAL;
          break;
        }

      /* Suppress the STOP when the next message continues this one */

      ret = s6_i2c_xfer_msg(priv, &msgs[i],
                            i + 1 < count &&
                            (msgs[i + 1].flags & I2C_M_NOSTART) != 0);
      if (ret < 0)
        {
          break;
        }
    }

  nxmutex_unlock(&priv->lock);
  return (ret < 0) ? ret : count;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: s6_i2cbus_initialize
 *
 * Description:
 *   Initialize the S6 I2C master (i2c0) and return its bus interface.
 *   The transfer engine is polled; no controller interrupt is used.
 *
 * Returned Value:
 *   The I2C0 master instance.
 *
 ****************************************************************************/

FAR struct i2c_master_s *s6_i2cbus_initialize(void)
{
  FAR struct s6_i2c_dev_s *priv = &g_s6_i2c0;

  /* Turn on the clock gate; a transfer triggers on the 0 -> 1 edge of
   * START, so leave it cleared with a known ACK/ERROR state.
   */

  modifyreg32(S6_CLK_SYS_CLK_EN0_REG1, 0, S6_I2C_GATE);
  modifyreg32(priv->base + S6_I2C_CTRL_OFFSET,
              S6_I2C_CTRL_START | S6_I2C_CTRL_ERROR |
              S6_I2C_CTRL_ACK_IGNORE, 0);

  s6_i2c_set_clk_div(priv, I2C_SPEED_STANDARD);
  priv->frequency = I2C_SPEED_STANDARD;

  iinfo("base %08lx\n", (unsigned long)priv->base);
  return &priv->dev;
}

#endif /* CONFIG_S6_I2C */
