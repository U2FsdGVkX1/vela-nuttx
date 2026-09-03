/****************************************************************************
 * arch/arm64/include/s6/chip.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_INCLUDE_S6_CHIP_H
#define __ARCH_ARM64_INCLUDE_S6_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#define KB(x) ((x) << 10)
#define MB(x) (KB(x) << 10)
#define GB(x) (MB(UINT64_C(x)) << 10)

/* Amlogic Meson S6 GICv3. */

#define CONFIG_GICD_BASE          0xff200000
#define CONFIG_GICR_BASE          0xff240000
#define CONFIG_GICR_OFFSET        0x20000

/* Initial flat-map windows. The heap is smaller and board-specific. */

#define CONFIG_RAMBANK1_ADDR      0x00000000
#define CONFIG_RAMBANK1_SIZE      GB(2)
#define CONFIG_DEVICEIO_BASEADDR  0xfe000000
#define CONFIG_DEVICEIO_SIZE      MB(32)

/* Final code address after U-Boot's ARM64 booti relocation. */

#define CONFIG_LOAD_BASE          0x03080000
#define MPID_TO_CLUSTER_ID(mpid)  ((mpid) & ~0xffUL)

#ifdef __ASSEMBLY__

/* S6 DTS uses Aff1 values 0x00, 0x01, 0x02 and 0x03 for the CPUs. */

.macro get_cpu_id xreg0
#ifdef CONFIG_SMP
  mrs   \xreg0, mpidr_el1
  ubfx  \xreg0, \xreg0, #8, #8
#else
  /* UP bring-up has no secondary-core path to reject. */

  mov   \xreg0, #0
#endif
.endm

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM64_INCLUDE_S6_CHIP_H */
