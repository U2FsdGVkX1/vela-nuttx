/****************************************************************************
 * arch/arm64/src/s6/hardware/s6_eth.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_S6_HARDWARE_S6_ETH_H
#define __ARCH_ARM64_SRC_S6_HARDWARE_S6_ETH_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Synopsys DWMAC (Ethernet QoS), GIC SPI 74.  The device tree says 4.00,
 * MAC_Version reads 5.30.
 */

#define S6_ETH_BASE                   0xfe368000
#define S6_ETH_IRQ                    106

/* Register offsets (relative to S6_ETH_BASE) */

#define S6_ETH_MAC_CONFIG             0x0000
#define S6_ETH_MAC_PACKET_FILTER      0x0008
#define S6_ETH_MAC_RXQ_CTRL0          0x00a0
#define S6_ETH_MAC_VERSION            0x0110
#define S6_ETH_MAC_MDIO_ADDR          0x0200
#define S6_ETH_MAC_MDIO_DATA          0x0204
#define S6_ETH_MAC_ADDR0_HIGH         0x0300
#define S6_ETH_MAC_ADDR0_LOW          0x0304
#define S6_ETH_MMC_RX_INTR_MASK       0x070c
#define S6_ETH_MMC_TX_INTR_MASK       0x0710
#define S6_ETH_MMC_IPC_RX_INTR_MASK   0x0800
#define S6_ETH_MTL_TXQ0_OP_MODE       0x0d00
#define S6_ETH_MTL_RXQ0_OP_MODE       0x0d30
#define S6_ETH_DMA_MODE               0x1000
#define S6_ETH_DMA_CH0_CONTROL        0x1100
#define S6_ETH_DMA_CH0_TX_CONTROL     0x1104
#define S6_ETH_DMA_CH0_RX_CONTROL     0x1108
#define S6_ETH_DMA_CH0_TXDESC_LIST    0x1114
#define S6_ETH_DMA_CH0_RXDESC_LIST    0x111c
#define S6_ETH_DMA_CH0_TXDESC_TAIL    0x1120
#define S6_ETH_DMA_CH0_RXDESC_TAIL    0x1128
#define S6_ETH_DMA_CH0_TXDESC_RING    0x112c
#define S6_ETH_DMA_CH0_RXDESC_RING    0x1130
#define S6_ETH_DMA_CH0_INTR_EN        0x1134
#define S6_ETH_DMA_CH0_STATUS         0x1160

/* MAC_Configuration */

#define S6_ETH_MAC_CONFIG_RE          (1u << 0)
#define S6_ETH_MAC_CONFIG_TE          (1u << 1)
#define S6_ETH_MAC_CONFIG_DM          (1u << 13)
#define S6_ETH_MAC_CONFIG_FES         (1u << 14)
#define S6_ETH_MAC_CONFIG_PS          (1u << 15)

/* MAC_RxQ_Ctrl0: queue 0 enabled for generic (DCB) traffic */

#define S6_ETH_MAC_RXQ0EN_DCB         (2u << 0)

/* MAC_Address0_High: AE is always set for address 0 */

#define S6_ETH_MAC_ADDR_AE            (1u << 31)

/* MAC_MDIO_Address */

#define S6_ETH_MDIO_GB                (1u << 0)
#define S6_ETH_MDIO_GOC_WRITE         (1u << 2)
#define S6_ETH_MDIO_GOC_READ          (3u << 2)
#define S6_ETH_MDIO_CR(n)             ((uint32_t)(n) << 8)
#define S6_ETH_MDIO_RDA(n)            ((uint32_t)(n) << 16)
#define S6_ETH_MDIO_PA(n)             ((uint32_t)(n) << 21)

/* MDC = clk_csr_i / 102 for a 150-250 MHz CSR clock (sys_clk) */

#define S6_ETH_MDIO_CR_150_250M       4

/* MTL_TxQ0_Operation_Mode / MTL_RxQ0_Operation_Mode.  The queue sizes
 * are in 256 byte blocks minus one (TX FIFO 2 KB, RX FIFO 4 KB).
 */

#define S6_ETH_MTL_TXQ_TSF            (1u << 1)
#define S6_ETH_MTL_TXQ_TXQEN          (2u << 2)
#define S6_ETH_MTL_TXQ_TQS(n)         ((uint32_t)(n) << 16)
#define S6_ETH_MTL_RXQ_RSF            (1u << 5)
#define S6_ETH_MTL_RXQ_RQS(n)         ((uint32_t)(n) << 20)

/* DMA_Mode */

#define S6_ETH_DMA_MODE_SWR           (1u << 0)

/* DMA_CH0_Control */

#define S6_ETH_DMA_CH_PBLX8           (1u << 16)

/* DMA_CH0_Tx_Control / DMA_CH0_Rx_Control */

#define S6_ETH_DMA_TX_ST              (1u << 0)
#define S6_ETH_DMA_TX_OSF             (1u << 4)
#define S6_ETH_DMA_TX_PBL(n)          ((uint32_t)(n) << 16)
#define S6_ETH_DMA_RX_SR              (1u << 0)
#define S6_ETH_DMA_RX_RBSZ(n)         ((uint32_t)(n) << 1)
#define S6_ETH_DMA_RX_PBL(n)          ((uint32_t)(n) << 16)

/* DMA_CH0_Interrupt_Enable.  NIE and AIE moved down one bit in 4.10. */

#define S6_ETH_DMA_INT_TIE            (1u << 0)
#define S6_ETH_DMA_INT_RIE            (1u << 6)
#define S6_ETH_DMA_INT_FBE            (1u << 12)
#define S6_ETH_DMA_INT_AIE            (1u << 15)
#define S6_ETH_DMA_INT_NIE            (1u << 16)
#define S6_ETH_DMA_INT_AIE_4_10       (1u << 14)
#define S6_ETH_DMA_INT_NIE_4_10       (1u << 15)

/* DMA_CH0_Status.  Bits [15:0] are write-1-to-clear, the bus error
 * fields above them are read-only.
 */

#define S6_ETH_DMA_STAT_TI            (1u << 0)
#define S6_ETH_DMA_STAT_RI            (1u << 6)
#define S6_ETH_DMA_STAT_FBE           (1u << 12)
#define S6_ETH_DMA_STAT_W1C           0xffffu

/* TX descriptor, read format */

#define S6_ETH_TDES2_IOC              (1u << 31)
#define S6_ETH_TDES3_FD               (1u << 29)
#define S6_ETH_TDES3_LD               (1u << 28)
#define S6_ETH_TDES3_OWN              (1u << 31)

/* TX descriptor, write-back format */

#define S6_ETH_TDES3_ES               (1u << 15)

/* RX descriptor, read format */

#define S6_ETH_RDES3_BUF1V            (1u << 24)
#define S6_ETH_RDES3_IOC              (1u << 30)
#define S6_ETH_RDES3_OWN              (1u << 31)

/* RX descriptor, write-back format */

#define S6_ETH_RDES3_PL_MASK          0x7fffu
#define S6_ETH_RDES3_ES               (1u << 15)
#define S6_ETH_RDES3_LD               (1u << 28)
#define S6_ETH_RDES3_FD               (1u << 29)

#endif /* __ARCH_ARM64_SRC_S6_HARDWARE_S6_ETH_H */
