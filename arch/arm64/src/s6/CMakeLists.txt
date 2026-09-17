# ##############################################################################
# arch/arm64/src/s6/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# ##############################################################################

set(SRCS s6_boot.c s6_serial.c)

if(CONFIG_ARCH_EARLY_PRINT)
  list(APPEND SRCS s6_lowputc.S)
endif()

if(CONFIG_S6_MIPI_DSI)
  list(APPEND SRCS s6_mipi_dsi.c)
endif()

target_sources(arch PRIVATE ${SRCS})
