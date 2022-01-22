/*
 * Copyright (c) 2012-2015 Travis Geiselbrecht
 * Copyright (c) 2022 Bingxing Wang
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <assert.h>
#include <sys/types.h>

#include <lk/bits.h>
#include <lk/debug.h>
#include <lk/err.h>
#include <lk/init.h>
#include <sys/types.h>

#include <dev/interrupt/arm_gic_v3.h>
#include <platform/interrupts.h>
#include <private/ArmGicLib.h>
#include <private/IoHighLevel.h>

#include <kernel/debug.h>
#include <kernel/thread.h>

#include <arch/ops.h>
#include <lk/trace.h>

#if WITH_LIB_SM
#error "Unsupported"
#endif

#if ARCH_ARM64
#include <arch/arm64.h>
#define iframe arm64_iframe_short
#define IFRAME_PC(frame) ((frame)->elr)
#define ARM_GIC_MAX_PER_CPU_INT 32
#define GICV3_MAX_INT 1026
#endif

#define LOCAL_TRACE 0

static spin_lock_t gicd_lock;
#if WITH_LIB_SM
#define GICD_LOCK_FLAGS SPIN_LOCK_FLAG_IRQ_FIQ
#else
#define GICD_LOCK_FLAGS SPIN_LOCK_FLAG_INTERRUPTS
#endif

UINTN mGicDistributorBase;
UINTN mGicRedistributorsBase;
UINTN mGicNumInterrupts;

static bool arm_gic_interrupt_change_allowed(int irq) { return true; }
static void suspend_resume_fiq(bool resume_gicc, bool resume_gicd) {}

struct int_handler_struct {
  int_handler handler;
  void *arg;
};

static struct int_handler_struct
    int_handler_table_per_cpu[ARM_GIC_MAX_PER_CPU_INT][SMP_MAX_CPUS];
static struct int_handler_struct
    int_handler_table_shared[GICV3_MAX_INT - ARM_GIC_MAX_PER_CPU_INT];

static struct int_handler_struct *get_int_handler(unsigned int vector,
                                                  uint cpu) {
  if (vector < ARM_GIC_MAX_PER_CPU_INT)
    return &int_handler_table_per_cpu[vector][cpu];
  else
    return &int_handler_table_shared[vector - ARM_GIC_MAX_PER_CPU_INT];
}

static bool gicv3_disable_interrupt_source(IN UINTN Source) {
  if (Source >= mGicNumInterrupts) {
    PANIC_UNIMPLEMENTED;
    return false;
  }

  ArmGicDisableInterrupt(mGicDistributorBase, mGicRedistributorsBase, Source);
  return true;
}

void arm_gicv3_init(uint64_t distributorBase, uint64_t redistributorBase) {
  mGicDistributorBase = distributorBase;
  mGicRedistributorsBase = redistributorBase;
  mGicNumInterrupts = ArmGicGetMaxNumInterrupts(mGicDistributorBase);

  // Drive it without compat mode
  MmioOr32(mGicDistributorBase + ARM_GIC_ICDDCR, ARM_GIC_ICDDCR_ARE);

  // Set Interrupt priority
  for (UINTN Index = 0; Index < mGicNumInterrupts; Index++) {
    gicv3_disable_interrupt_source(Index);

    // Set Priority
    ArmGicSetInterruptPriority(mGicDistributorBase, mGicRedistributorsBase,
                               Index, ARM_GIC_DEFAULT_PRIORITY);
  }

  // Targets the interrupts to the Primary Cpu
  uint64_t MpId = ARM64_READ_SYSREG(mpidr_el1);
  uint64_t CpuTarget =
      MpId & (ARM_CORE_AFF0 | ARM_CORE_AFF1 | ARM_CORE_AFF2 | ARM_CORE_AFF3);

  if ((MmioRead32(mGicDistributorBase + ARM_GIC_ICDDCR) & ARM_GIC_ICDDCR_DS) !=
      0) {
    // If the Disable Security (DS) control bit is set, we are dealing with a
    // GIC that has only one security state. In this case, let's assume we are
    // executing in non-secure state (which is appropriate for DXE modules)
    // and that no other firmware has performed any configuration on the GIC.
    // This means we need to reconfigure all interrupts to non-secure Group 1
    // first.

    MmioWrite32(mGicRedistributorsBase + ARM_GICR_CTLR_FRAME_SIZE +
                    ARM_GIC_ICDISR,
                0xffffffff);

    for (UINTN Index = 32; Index < mGicNumInterrupts; Index += 32) {
      MmioWrite32(mGicDistributorBase + ARM_GIC_ICDISR + Index / 8, 0xffffffff);
    }
  }

  // Route the SPIs to the primary CPU. SPIs start at the INTID 32
  for (UINTN Index = 0; Index < (mGicNumInterrupts - 32); Index++) {
    MmioWrite64(mGicDistributorBase + ARM_GICD_IROUTER + (Index * 8),
                CpuTarget);
  }

  // Set binary point reg to 0x7 (no preemption)
  ArmGicV3SetBinaryPointer(0x7);

  // Set priority mask reg to 0xff to allow all priorities through
  ArmGicV3SetPriorityMask(0xff);

  // Enable gic cpu interface
  ArmGicV3EnableInterruptInterface();

  // Enable gic distributor
  ArmGicEnableDistributor(mGicDistributorBase);
}

void register_int_handler(unsigned int vector, int_handler handler, void *arg) {
  struct int_handler_struct *h;
  uint cpu = arch_curr_cpu_num();

  spin_lock_saved_state_t state;

  if (vector >= GICV3_MAX_INT || vector >= mGicNumInterrupts) {
    panic("register_int_handler: vector out of range %d\n", vector);
  }

  spin_lock_save(&gicd_lock, &state, GICD_LOCK_FLAGS);

  if (arm_gic_interrupt_change_allowed(vector)) {
    h = get_int_handler(vector, cpu);
    h->handler = handler;
    h->arg = arg;
  }

  spin_unlock_restore(&gicd_lock, state, GICD_LOCK_FLAGS);
}

status_t mask_interrupt(unsigned int vector) {
  if (vector >= mGicNumInterrupts) {
    PANIC_UNIMPLEMENTED;
    return ERR_NOT_VALID;
  }

  ArmGicDisableInterrupt(mGicDistributorBase, mGicRedistributorsBase, vector);
  return NO_ERROR;
}

status_t unmask_interrupt(unsigned int vector) {
  if (vector >= mGicNumInterrupts) {
    PANIC_UNIMPLEMENTED;
    return ERR_NOT_VALID;
  }

  ArmGicEnableInterrupt(mGicDistributorBase, mGicRedistributorsBase, vector);
  return NO_ERROR;
}

static enum handler_return __platform_irq(struct iframe *frame) {
  UINT32 GicInterrupt;
  GicInterrupt = ArmGicV3AcknowledgeInterrupt();

  unsigned int vector = GicInterrupt & 0x3ff;
  if (vector >= 0x3fe) {
    // spurious
    return INT_NO_RESCHEDULE;
  }

  THREAD_STATS_INC(interrupts);
  KEVLOG_IRQ_ENTER(vector);

  uint cpu = arch_curr_cpu_num();

  LTRACEF_LEVEL(2, "iar 0x%x cpu %u currthread %p vector %d pc 0x%lx\n",
                GicInterrupt, cpu, get_current_thread(), vector,
                (uintptr_t)IFRAME_PC(frame));

  // deliver the interrupt
  enum handler_return ret;

  ret = INT_NO_RESCHEDULE;
  struct int_handler_struct *handler = get_int_handler(vector, cpu);
  if (handler->handler)
    ret = handler->handler(handler->arg);

  ArmGicV3EndOfInterrupt(vector);
  LTRACEF_LEVEL(2, "cpu %u exit %d\n", cpu, ret);
  KEVLOG_IRQ_EXIT(vector);

  return ret;
}

enum handler_return platform_irq(struct iframe *frame);
enum handler_return platform_irq(struct iframe *frame) {
  return __platform_irq(frame);
}

void platform_fiq(struct iframe *frame);
void platform_fiq(struct iframe *frame) {
  // TODO: The only possible case for FIQ right now is the timer
  PANIC_UNIMPLEMENTED_MSG("Enter FIQ");
}
