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

#include <dev/interrupt/arm_gic_v3.h>
#include <dev/timer/arm_generic.h>
#include <dev/uart.h>

#include <lk/init.h>

#include <kernel/spinlock.h>
#include <kernel/vm.h>

#include <platform.h>
#include <platform/hv-gen2.h>
#include <platform/interrupts.h>

#include "platform_p.h"

extern int psci_call(ulong arg0, ulong arg1, ulong arg2, ulong arg3);

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
  /* GIC, Timer and then UART */
  uart_init_early();
  printf("Bring up GICv3\n");
  arm_gicv3_init(HV_GICD_ADDRESS, HV_GICR_BASE);

  /* Hyper-V UEFI still uses stimer0, we need to bring this up or it will blow
   * up. And we use the virt timer */
  printf("Bring up Timer\n");
  arm_generic_timer_init(ARM_GENERIC_TIMER_VIRTUAL_INT, 0);

  /* add the main memory arena */
  pmm_add_arena(&arena);

  /* hardcoded */
  LTRACEF("booting %d cpus\n", 1);
}

void platform_init(void) {
  uart_init();
  /* That's all */
}

#define ARM_SMC_ID_PSCI_SYSTEM_OFF 0x84000008
#define ARM_SMC_ID_PSCI_SYSTEM_RESET 0x84000009

void __NO_RETURN platform_halt(platform_halt_action suggested_action,
                               platform_halt_reason reason) {
  switch (suggested_action) {
  case HALT_ACTION_REBOOT:
    printf("PSCI call for reset\n");
    psci_call(ARM_SMC_ID_PSCI_SYSTEM_RESET, 0, 0, 0);
    break;
  case HALT_ACTION_SHUTDOWN:
    printf("PSCI call for power off\n");
    psci_call(ARM_SMC_ID_PSCI_SYSTEM_OFF, 0, 0, 0);
    break;
  }

  // Unless it's halt, we should not reach here
  printf("HALT: spinning forever... (reason = %d)\n", reason);
  arch_disable_ints();
  for (;;) {
    arch_idle();
  }
}
