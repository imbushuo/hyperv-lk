// Hyper-V GICv3 implementation

/*
 * Copyright (c) 2012-2015 Travis Geiselbrecht
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
#include <sys/types.h>

#include <ArmGicLib.h>
#include <gicv3.h>

#include <IoHighLevel.h>

#include <kernel/debug.h>
#include <kernel/thread.h>

#include <lk/init.h>

#include <arch/ops.h>
#include <lk/trace.h>

#include <platform/hv-gic.h>
#include <platform/interrupts.h>

#if WITH_LIB_SM
#error "Unsupported"
#endif

#if ARCH_ARM64
#include <arch/arm64.h>
#define iframe arm64_iframe_short
#define IFRAME_PC(frame) ((frame)->elr)

#define ARM_CORE_AFF0 0xFF
#define ARM_CORE_AFF1 (0xFF << 8)
#define ARM_CORE_AFF2 (0xFF << 16)
#define ARM_CORE_AFF3 (0xFFULL << 32)
#endif

#define GIC_MAX_PER_CPU_INT 32

/* BEGIN EDK2 content */

// In GICv3, there are 2 x 64KB frames:
// Redistributor control frame + SGI Control & Generation frame
#define GIC_V3_REDISTRIBUTOR_GRANULARITY                                       \
  (ARM_GICR_CTLR_FRAME_SIZE + ARM_GICR_SGI_PPI_FRAME_SIZE)

// In GICv4, there are 2 additional 64KB frames:
// VLPI frame + Reserved page frame
#define GIC_V4_REDISTRIBUTOR_GRANULARITY                                       \
  (GIC_V3_REDISTRIBUTOR_GRANULARITY + ARM_GICR_SGI_VLPI_FRAME_SIZE +           \
   ARM_GICR_SGI_RESERVED_FRAME_SIZE)

#define ISENABLER_ADDRESS(base, offset)                                        \
  ((base) + ARM_GICR_CTLR_FRAME_SIZE + ARM_GICR_ISENABLER + 4 * (offset))

#define ICENABLER_ADDRESS(base, offset)                                        \
  ((base) + ARM_GICR_CTLR_FRAME_SIZE + ARM_GICR_ICENABLER + 4 * (offset))

#define IPRIORITY_ADDRESS(base, offset)                                        \
  ((base) + ARM_GICR_CTLR_FRAME_SIZE + ARM_GIC_ICDIPR + 4 * (offset))

#define ARM_GIC_DEFAULT_PRIORITY 0x80

static UINTN mGicDistributorBase;
static UINTN mGicRedistributorsBase;
static UINTN mGicNumInterrupts;

static BOOLEAN SourceIsSpi(IN UINTN Source) {
  return Source >= 32 && Source < 1020;
}

static UINTN GicGetCpuRedistributorBase(IN UINTN GicRedistributorBase) {
  UINTN MpId;
  UINTN CpuAffinity;
  UINTN Affinity;
  UINTN GicCpuRedistributorBase;
  uint64_t TypeRegister;

  MpId = ARM64_READ_SYSREG(mpidr_el1);
  // Define CPU affinity as:
  // Affinity0[0:8], Affinity1[9:15], Affinity2[16:23], Affinity3[24:32]
  // whereas Affinity3 is defined at [32:39] in MPIDR
  CpuAffinity = (MpId & (ARM_CORE_AFF0 | ARM_CORE_AFF1 | ARM_CORE_AFF2)) |
                ((MpId & ARM_CORE_AFF3) >> 8);

  GicCpuRedistributorBase = GicRedistributorBase;

  do {
    TypeRegister = MmioRead64(GicCpuRedistributorBase + ARM_GICR_TYPER);
    Affinity = ARM_GICR_TYPER_GET_AFFINITY(TypeRegister);
    if (Affinity == CpuAffinity) {
      return GicCpuRedistributorBase;
    }

    // Move to the next GIC Redistributor frame.
    // The GIC specification does not forbid a mixture of redistributors
    // with or without support for virtual LPIs, so we test Virtual LPIs
    // Support (VLPIS) bit for each frame to decide the granularity.
    // Note: The assumption here is that the redistributors are adjacent
    // for all CPUs. However this may not be the case for NUMA systems.
    GicCpuRedistributorBase += (((ARM_GICR_TYPER_VLPIS & TypeRegister) != 0)
                                    ? GIC_V4_REDISTRIBUTOR_GRANULARITY
                                    : GIC_V3_REDISTRIBUTOR_GRANULARITY);
  } while ((TypeRegister & ARM_GICR_TYPER_LAST) == 0);

  // The Redistributor has not been found for the current CPU
  PANIC_UNIMPLEMENTED_MSG(
      "The Redistributor has not been found for the current CPU");
  return 0;
}

static bool EFIAPI GicV3DisableInterruptSource(IN UINTN Source) {
  if (Source >= mGicNumInterrupts) {
    PANIC_UNIMPLEMENTED;
    return false;
  }

  ArmGicDisableInterrupt(mGicDistributorBase, mGicRedistributorsBase, Source);
  return true;
}

UINTN
EFIAPI
ArmGicGetMaxNumInterrupts(IN INTN GicDistributorBase) {
  UINTN ItLines;

  ItLines = MmioRead32(GicDistributorBase + ARM_GIC_ICDICTR) & 0x1F;

  //
  // Interrupt ID 1020-1023 are reserved.
  //
  return (ItLines == 0x1f) ? 1020 : 32 * (ItLines + 1);
}

VOID EFIAPI ArmGicEnableInterrupt(IN UINTN GicDistributorBase,
                                  IN UINTN GicRedistributorBase,
                                  IN UINTN Source) {
  UINT32 RegOffset;
  UINTN RegShift;
  UINTN GicCpuRedistributorBase;

  // Calculate enable register offset and bit position
  RegOffset = Source / 32;
  RegShift = Source % 32;

  if (SourceIsSpi(Source)) {
    // Write set-enable register
    MmioWrite32(GicDistributorBase + ARM_GIC_ICDISER + (4 * RegOffset),
                1 << RegShift);
  } else {
    GicCpuRedistributorBase = GicGetCpuRedistributorBase(GicRedistributorBase);
    if (GicCpuRedistributorBase == 0) {
      PANIC_UNIMPLEMENTED;
      return;
    }

    // Write set-enable register
    MmioWrite32(ISENABLER_ADDRESS(GicCpuRedistributorBase, RegOffset),
                1 << RegShift);
  }
}

VOID EFIAPI ArmGicDisableInterrupt(IN UINTN GicDistributorBase,
                                   IN UINTN GicRedistributorBase,
                                   IN UINTN Source) {
  UINT32 RegOffset;
  UINTN RegShift;
  UINTN GicCpuRedistributorBase;

  // Calculate enable register offset and bit position
  RegOffset = Source / 32;
  RegShift = Source % 32;

  if (SourceIsSpi(Source)) {
    // Write clear-enable register
    MmioWrite32(GicDistributorBase + ARM_GIC_ICDICER + (4 * RegOffset),
                1 << RegShift);
  } else {
    GicCpuRedistributorBase = GicGetCpuRedistributorBase(GicRedistributorBase);
    if (GicCpuRedistributorBase == 0) {
      PANIC_UNIMPLEMENTED;
      return;
    }

    // Write clear-enable register
    MmioWrite32(ICENABLER_ADDRESS(GicCpuRedistributorBase, RegOffset),
                1 << RegShift);
  }
}

VOID EFIAPI ArmGicSetInterruptPriority(IN UINTN GicDistributorBase,
                                       IN UINTN GicRedistributorBase,
                                       IN UINTN Source, IN UINTN Priority) {
  UINT32 RegOffset;
  UINTN RegShift;
  UINTN GicCpuRedistributorBase;

  // Calculate register offset and bit position
  RegOffset = Source / 4;
  RegShift = (Source % 4) * 8;

  if (SourceIsSpi(Source)) {
    MmioAndThenOr32(GicDistributorBase + ARM_GIC_ICDIPR + (4 * RegOffset),
                    ~(0xff << RegShift), Priority << RegShift);
  } else {
    GicCpuRedistributorBase = GicGetCpuRedistributorBase(GicRedistributorBase);
    if (GicCpuRedistributorBase == 0) {
      return;
    }

    MmioAndThenOr32(IPRIORITY_ADDRESS(GicCpuRedistributorBase, RegOffset),
                    ~(0xff << RegShift), Priority << RegShift);
  }
}

VOID EFIAPI ArmGicEnableDistributor(IN INTN GicDistributorBase) {
  /*
   * Enable GIC distributor in Non-Secure world.
   * Note: The ICDDCR register is banked when Security extensions are
   * implemented
   */
  if (MmioRead32(GicDistributorBase + ARM_GIC_ICDDCR) & ARM_GIC_ICDDCR_ARE) {
    MmioOr32(GicDistributorBase + ARM_GIC_ICDDCR, 0x2);
  } else {
    MmioOr32(GicDistributorBase + ARM_GIC_ICDDCR, 0x1);
  }
}

/* BEGIN LK content */
#define LOCAL_TRACE 0

static spin_lock_t gicd_lock;
#if WITH_LIB_SM
#define GICD_LOCK_FLAGS SPIN_LOCK_FLAG_IRQ_FIQ
#else
#define GICD_LOCK_FLAGS SPIN_LOCK_FLAG_INTERRUPTS
#endif

static bool arm_gic_interrupt_change_allowed(int irq) { return true; }
static void suspend_resume_fiq(bool resume_gicc, bool resume_gicd) {}

struct int_handler_struct {
  int_handler handler;
  void *arg;
};

static struct int_handler_struct int_handler_table_per_cpu[GIC_MAX_PER_CPU_INT]
                                                          [SMP_MAX_CPUS];
static struct int_handler_struct
    int_handler_table_shared[MAX_INT - GIC_MAX_PER_CPU_INT];

static struct int_handler_struct *get_int_handler(unsigned int vector,
                                                  uint cpu) {
  if (vector < GIC_MAX_PER_CPU_INT)
    return &int_handler_table_per_cpu[vector][cpu];
  else
    return &int_handler_table_shared[vector - GIC_MAX_PER_CPU_INT];
}

void arm_gicv3_init(void) {
  mGicDistributorBase = HV_GICD_ADDRESS;
  mGicRedistributorsBase = HV_GICR_BASE;
  mGicNumInterrupts = ArmGicGetMaxNumInterrupts(mGicDistributorBase);

  // Drive it without compat mode
  MmioOr32(mGicDistributorBase + ARM_GIC_ICDDCR, ARM_GIC_ICDDCR_ARE);

  // Set Interrupt priority
  for (UINTN Index = 0; Index < mGicNumInterrupts; Index++) {
    GicV3DisableInterruptSource(Index);

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

  if (vector >= MAX_INT || vector >= mGicNumInterrupts) {
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
void platform_fiq(struct iframe *frame) { PANIC_UNIMPLEMENTED; }
