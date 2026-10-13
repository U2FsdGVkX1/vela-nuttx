/****************************************************************************
 * arch/arm64/src/s6/s6_i2c.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_S6_S6_I2C_H
#define __ARCH_ARM64_SRC_S6_S6_I2C_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>

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

/****************************************************************************
 * Name: s6_i2cbus_initialize
 *
 * Description:
 *   Initialize the S6 I2C master (i2c0) and return its bus interface.
 *
 * Returned Value:
 *   The I2C0 master instance.
 *
 ****************************************************************************/

EXTERN FAR struct i2c_master_s *s6_i2cbus_initialize(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM64_SRC_S6_S6_I2C_H */
