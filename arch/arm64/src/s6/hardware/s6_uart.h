/****************************************************************************
 * arch/arm64/src/s6/hardware/s6_uart.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_S6_HARDWARE_S6_UART_H
#define __ARCH_ARM64_SRC_S6_HARDWARE_S6_UART_H

#define S6_UART_WFIFO_OFFSET       0x00
#define S6_UART_RFIFO_OFFSET       0x04
#define S6_UART_CONTROL_OFFSET     0x08
#define S6_UART_STATUS_OFFSET      0x0c
#define S6_UART_MISC_OFFSET        0x10
#define S6_UART_REG5_OFFSET        0x14

#define S6_UART_CONTROL_TX_EN      (1u << 12)
#define S6_UART_CONTROL_RX_EN      (1u << 13)
#define S6_UART_CONTROL_TWO_WIRE   (1u << 15)
#define S6_UART_CONTROL_STOP_2     (1u << 16)
#define S6_UART_CONTROL_PARITY_ODD (1u << 18)
#define S6_UART_CONTROL_PARITY_EN  (1u << 19)
#define S6_UART_CONTROL_DATA_LEN_MASK  (3u << 20)
#define S6_UART_CONTROL_DATA_LEN_8BIT  (0u << 20)
#define S6_UART_CONTROL_DATA_LEN_7BIT  (1u << 20)
#define S6_UART_CONTROL_DATA_LEN_6BIT  (2u << 20)
#define S6_UART_CONTROL_DATA_LEN_5BIT  (3u << 20)
#define S6_UART_CONTROL_TX_RST     (1u << 22)
#define S6_UART_CONTROL_RX_RST     (1u << 23)
#define S6_UART_CONTROL_CLR_ERR    (1u << 24)
#define S6_UART_CONTROL_RX_INT_EN  (1u << 27)
#define S6_UART_CONTROL_TX_INT_EN  (1u << 28)

#define S6_UART_STATUS_PARITY_ERR  (1u << 16)
#define S6_UART_STATUS_FRAME_ERR   (1u << 17)
#define S6_UART_STATUS_TX_WERR     (1u << 18)
#define S6_UART_STATUS_RX_EMPTY    (1u << 20)
#define S6_UART_STATUS_TX_FULL     (1u << 21)
#define S6_UART_STATUS_TX_EMPTY    (1u << 22)
#define S6_UART_STATUS_RX_OVERFLOW (1u << 24)

#define S6_UART_REG5_BAUD_MASK     0x007fffffu
#define S6_UART_REG5_USE_NEW_BAUD  (1u << 23)
#define S6_UART_REG5_USE_XTAL      (1u << 24)
#define S6_UART_REG5_XTAL_DIV2     (1u << 27)

#define S6_UART_MISC_RX_LEVEL(n)   ((n) & 0xffu)
#define S6_UART_MISC_TX_LEVEL(n)   (((n) & 0xffu) << 8)

#define S6_UART_BAUD_CLOCK         12000000u
#define S6_UART_BAUD_DIVISOR(baud) \
  ((S6_UART_BAUD_CLOCK + ((baud) / 2u)) / (baud) - 1u)

#endif /* __ARCH_ARM64_SRC_S6_HARDWARE_S6_UART_H */
