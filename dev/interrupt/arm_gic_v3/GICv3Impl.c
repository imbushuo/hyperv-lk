// GICv3 implementation support carried from EDK2

#include <assert.h>
#include <sys/types.h>

#include <lk/bits.h>
#include <lk/debug.h>
#include <lk/err.h>
#include <lk/init.h>
#include <lk/trace.h>

#include <kernel/debug.h>
#include <kernel/thread.h>

#include <private/IoHighLevel.h>

#include <private/ArmGicLib.h>

#include <arch/ops.h>
#if ARCH_ARM64
#include <arch/arm64.h>
#define iframe arm64_iframe_short
#define IFRAME_PC(frame) ((frame)->elr)
#else
#error "Unsupported"
#endif

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

bool EFIAPI GicV3DisableInterruptSource(IN UINTN Source) {
  if (Source >= mGicNumInterrupts) {
    PANIC_UNIMPLEMENTED;
    return false;
  }

  ArmGicDisableInterrupt(mGicDistributorBase, mGicRedistributorsBase, Source);
  return true;
}

bool EFIAPI GicV3EnableInterruptSource(IN UINTN Source) {
  if (Source >= mGicNumInterrupts) {
    PANIC_UNIMPLEMENTED;
    return false;
  }

  ArmGicEnableInterrupt(mGicDistributorBase, mGicRedistributorsBase, Source);
  return true;
}
