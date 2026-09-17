/****************************************************************************
 * arch/arm64/src/s6/hardware/s6_i2c.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_S6_HARDWARE_S6_I2C_H
#define __ARCH_ARM64_SRC_S6_HARDWARE_S6_I2C_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <arch/chip/chip.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* I2C token commands, four bits each, packed into the token list
 * registers (up to 8 tokens per register).
 */

enum s6_i2c_token_e
{
  S6_I2C_TOKEN_END              = 0,
  S6_I2C_TOKEN_START            = 1,
  S6_I2C_TOKEN_SLAVE_ADDR_WRITE = 2,
  S6_I2C_TOKEN_SLAVE_ADDR_READ  = 3,
  S6_I2C_TOKEN_DATA             = 4,
  S6_I2C_TOKEN_DATA_LAST        = 5,
  S6_I2C_TOKEN_STOP             = 6,
};

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Register offsets (relative to the controller base address) */

#define S6_I2C_CTRL_OFFSET        0x00
#define S6_I2C_SLAVE_ADDR_OFFSET  0x04
#define S6_I2C_TOK_LIST0_OFFSET   0x08
#define S6_I2C_TOK_LIST1_OFFSET   0x0c
#define S6_I2C_TOK_WDATA0_OFFSET  0x10
#define S6_I2C_TOK_WDATA1_OFFSET  0x14
#define S6_I2C_TOK_RDATA0_OFFSET  0x18
#define S6_I2C_TOK_RDATA1_OFFSET  0x1c

/* CTRL register fields */

#define S6_I2C_CTRL_START         (1u << 0)
#define S6_I2C_CTRL_ACK_IGNORE    (1u << 1)
#define S6_I2C_CTRL_STATUS        (1u << 2)
#define S6_I2C_CTRL_ERROR         (1u << 3)
#define S6_I2C_CTRL_SCL_LEVEL     (1u << 25)
#define S6_I2C_CTRL_SDA_LEVEL     (1u << 26)
#define S6_I2C_CTRL_CLKDIV_SHIFT  12
#define S6_I2C_CTRL_CLKDIV_MASK   (0x3ffu << S6_I2C_CTRL_CLKDIV_SHIFT)
#define S6_I2C_CTRL_CLKDIVEXT_SHIFT 28
#define S6_I2C_CTRL_CLKDIVEXT_MASK  (0x3u << S6_I2C_CTRL_CLKDIVEXT_SHIFT)

/* SLAVE_ADDR register fields.  Bits [7:0] hold the slave address (addr << 1)
 * while bits [27:16] hold the SCL low time divisor and bit 28 enables it,
 * so every access must use masking.
 */

#define S6_I2C_SLV_ADDR_MASK      0xffu
#define S6_I2C_SLV_DIVL_SHIFT     16
#define S6_I2C_SLV_DIVL_MASK      (0xfffu << S6_I2C_SLV_DIVL_SHIFT)
#define S6_I2C_SLV_DIVL_EN        (1u << 28)

/* SYS_CLK_EN0_REG1 gate bits.  sys_i2c_m_a is the parent of all the
 * master gates and must be on for any of them to run.
 */

#define S6_CLK_SYS_CLK_EN0_REG1   (CONFIG_DEVICEIO_BASEADDR + 0x48)
#define S6_I2C_GATE               (1u << 30)

#endif /* __ARCH_ARM64_SRC_S6_HARDWARE_S6_I2C_H */
