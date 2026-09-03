/****************************************************************************
 * arch/arm64/src/s6/s6_boot.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <arch/board/board.h>
#include <arch/chip/chip.h>

#include "arm64_internal.h"
#include "arm64_mmu.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct arm_mmu_region g_mmu_regions[] =
{
  MMU_REGION_FLAT_ENTRY("S6_DEVICE_REGION",
                        CONFIG_DEVICEIO_BASEADDR,
                        CONFIG_DEVICEIO_SIZE,
                        MT_DEVICE_NGNRNE | MT_RW | MT_SECURE),

  MMU_REGION_FLAT_ENTRY("S6_DRAM_REGION",
                        CONFIG_RAMBANK1_ADDR,
                        CONFIG_RAMBANK1_SIZE,
                        MT_NORMAL | MT_RW | MT_SECURE),
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

const struct arm_mmu_config g_mmu_config =
{
  .num_regions = nitems(g_mmu_regions),
  .mmu_regions = g_mmu_regions,
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void arm64_el_init(void)
{
  uint64_t reg;

  if (arm64_current_el() != MODE_EL2)
    {
      return;
    }

  /* Clear the EL1 translation regime before ERET.  The common EL1 setup
   * runs only after the exception-level switch.
   */

  reg = SCTLR_EL1_RES1 | SCTLR_SA_BIT;
#ifndef CONFIG_ARM64_ICACHE_DISABLE
  reg |= SCTLR_I_BIT;
#endif
  write_sysreg(reg, sctlr_el1);

  /* U-Boot can also leave VHE or stage-2 translation enabled. */

  write_sysreg(HCR_RW_BIT, hcr_el2);
  zero_sysreg(vttbr_el2);
  UP_ISB();
}

void arm64_chip_boot(void)
{
  arm64_mmu_init(true);

#ifdef CONFIG_ARM64_PSCI
  arm64_psci_init("smc");
#endif

  s6_board_initialize();

#ifdef USE_EARLYSERIALINIT
  arm64_earlyserialinit();
#endif
}
