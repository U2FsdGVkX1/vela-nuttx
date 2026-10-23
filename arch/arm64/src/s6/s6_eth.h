/****************************************************************************
 * arch/arm64/src/s6/s6_eth.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_S6_S6_ETH_H
#define __ARCH_ARM64_SRC_S6_S6_ETH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

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
 * Name: s6_eth_initialize
 *
 * Description:
 *   Register the S6 Ethernet MAC as eth0.  The clocks, pins, PHY reset
 *   and RGMII delays are left as U-Boot configured them.
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

EXTERN int s6_eth_initialize(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM64_SRC_S6_S6_ETH_H */
