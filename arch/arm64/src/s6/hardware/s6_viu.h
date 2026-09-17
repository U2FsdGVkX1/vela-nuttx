/****************************************************************************
 * arch/arm64/src/s6/hardware/s6_viu.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_S6_HARDWARE_S6_VIU_H
#define __ARCH_ARM64_SRC_S6_HARDWARE_S6_VIU_H

/* Vendor "VCBUS" space: physical address = base + (index << 2). */

#define S6_VCBUS_BASE                 0xff000000u
#define S6_VCBUS_ADDR(idx)            (S6_VCBUS_BASE + ((idx) << 2))

/* VIU_OSD1 (fetched by the linear MIF, no canvas). */

#define S6_VIU_OSD1_CTRL_STAT         S6_VCBUS_ADDR(0x1a10)
#define S6_VIU_OSD1_BLK0_CFG_W1       S6_VCBUS_ADDR(0x1a1c)
#define S6_VIU_OSD1_BLK0_CFG_W2       S6_VCBUS_ADDR(0x1a1d)
#define S6_VIU_OSD1_BLK0_CFG_W3       S6_VCBUS_ADDR(0x1a1e)
#define S6_VIU_OSD1_BLK0_CFG_W4       S6_VCBUS_ADDR(0x1a13)
#define S6_VIU_OSD1_BLK1_CFG_W4       S6_VCBUS_ADDR(0x1a14)
#define S6_VIU_OSD1_BLK2_CFG_W4       S6_VCBUS_ADDR(0x1a15)
#define S6_VPP_OSD_SC_CTRL0           S6_VCBUS_ADDR(0x1dc8)

/* VIU_OSD1_CTRL_STAT bit 0: layer enable (FBIOSET_POWER). */

#define S6_OSD_CTRL_EN                (1u << 0)

/* Source (pan) and display windows, start/end inclusive. */

#define S6_OSD_WIN_X(end, start)      (((start) & 0x1fffu) | \
                                       (((end) & 0x1fffu) << 16))
#define S6_OSD_WIN_Y(end, start)      S6_OSD_WIN_X(end, start)
#define S6_OSD_DISP_X(end, start)     (((start) & 0xfffu) | \
                                       (((end) & 0xfffu) << 16))
#define S6_OSD_DISP_Y(end, start)     S6_OSD_DISP_X(end, start)

/* Frame address and line stride, both in 16-byte units. */

#define S6_OSD_MIF_ADDR(addr)         ((addr) >> 4)
#define S6_OSD_MIF_STRIDE_MASK        0xfffu
#define S6_OSD_MIF_STRIDE(bytes)      ((((bytes) + 15u) / 16u) & \
                                       S6_OSD_MIF_STRIDE_MASK)

#endif /* __ARCH_ARM64_SRC_S6_HARDWARE_S6_VIU_H */
