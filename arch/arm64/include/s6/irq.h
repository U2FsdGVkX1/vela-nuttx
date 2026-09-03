/****************************************************************************
 * arch/arm64/include/s6/irq.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

/* This file is included indirectly through arch/irq.h. */

#ifndef __ARCH_ARM64_INCLUDE_S6_IRQ_H
#define __ARCH_ARM64_INCLUDE_S6_IRQ_H

#define NR_IRQS 512

/* S6 CPU IDs are encoded in MPIDR Aff1. */

#define MPID_TO_CORE(mpid) \
  (((mpid) >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK)

#endif /* __ARCH_ARM64_INCLUDE_S6_IRQ_H */
