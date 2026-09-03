/****************************************************************************
 * arch/arm64/src/s6/s6_lowputc.S
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include "arm64_macro.inc"

    .file "s6_lowputc.S"

#define S6_UART_STATUS_OFFSET 0x0c
#define S6_UART_WFIFO_OFFSET  0x00
#define S6_UART_TX_FULL       0x00200000

GTEXT(arm64_earlyprintinit)
SECTION_FUNC(text, arm64_earlyprintinit)
    /* U-Boot has already configured the UART clock and pin mux. */
    ret

GTEXT(arm64_lowputc)
SECTION_FUNC(text, arm64_lowputc)
1:
    ldr   x15, =CONFIG_S6_UART0_BASE
    ldr   w2, [x15, #S6_UART_STATUS_OFFSET]
    tst   w2, #S6_UART_TX_FULL
    b.ne  1b
    str   w0, [x15, #S6_UART_WFIFO_OFFSET]
    dsb   sy
    ret
