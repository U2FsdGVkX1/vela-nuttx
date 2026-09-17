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

target_sources(arch PRIVATE ${SRCS})
