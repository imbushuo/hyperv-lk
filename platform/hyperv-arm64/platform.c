/*
 * Copyright (c) 2012-2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <arch.h>

#include <inttypes.h>

#include <lk/debug.h>
#include <lk/err.h>
#include <lk/trace.h>

#include <dev/timer/arm_generic.h>
#include <dev/uart.h>

#include <lk/init.h>

#include <kernel/spinlock.h>
#include <kernel/vm.h>

#include <gicv3.h>
#include <platform.h>
#include <platform/gic.h>
#include <platform/hv-gen2.h>
#include <platform/interrupts.h>

#include "platform_p.h"

#if WITH_LIB_MINIP
#include <lib/minip.h>
#endif

#define LOCAL_TRACE 0

#define DEFAULT_MEMORY_SIZE                                                    \
  (MEMSIZE) /* limit it yet due to Hyper-V arrangements */

/* initial memory mappings. parsed by start.S */
struct mmu_initial_mapping mmu_initial_mappings[] = {
    /* all of memory */
    {
        .phys = MEMORY_BASE_PHYS,
        .virt = KERNEL_BASE,
        .size = MEMORY_APERTURE_SIZE,
        .flags = 0,
        .name = "memory",
    },

    /* 1GB of peripherals */
    {
        .phys = PERIPHERAL_BASE_PHYS,
        .virt = PERIPHERAL_BASE_VIRT,
        .size = PERIPHERAL_BASE_SIZE,
        .flags = MMU_INITIAL_MAPPING_FLAG_DEVICE,
        .name = "peripherals",
    },

    /* null entry to terminate the list */
    {0},
};

static pmm_arena_t arena = {
    .name = "ram",
    .base = MEMORY_BASE_PHYS,
    .size = DEFAULT_MEMORY_SIZE,
    .flags = PMM_ARENA_FLAG_KMAP,
};

void platform_early_init(void) {
  while (true) {
  }
}

void platform_init(void) {
  while (true) {
  }
}
