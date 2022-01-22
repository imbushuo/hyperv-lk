#
#  Copyright (c) 2014, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
#

#include <lk/asm.h>
#include <private/AsmMacroIoLibV8.h>

.text

#if !defined(__clang__)

//
// Clang versions before v3.6 do not support the GNU extension that allows
// system registers outside of the IMPLEMENTATION DEFINED range to be specified
// using the generic notation below. However, clang knows these registers by
// their architectural names, so it has no need for these aliases anyway.
//
#define ICC_SRE_EL1             S3_0_C12_C12_5
#define ICC_SRE_EL2             S3_4_C12_C9_5
#define ICC_SRE_EL3             S3_6_C12_C12_5
#define ICC_IGRPEN1_EL1         S3_0_C12_C12_7
#define ICC_EOIR1_EL1           S3_0_C12_C12_1
#define ICC_IAR1_EL1            S3_0_C12_C12_0
#define ICC_PMR_EL1             S3_0_C4_C6_0
#define ICC_BPR1_EL1            S3_0_C12_C12_3

#endif

//UINT32
//EFIAPI
//ArmGicV3GetControlSystemRegisterEnable (
//  VOID
//  );
FUNCTION(ArmGicV3GetControlSystemRegisterEnable)
        mrs x0, ICC_SRE_EL1
        ret

//VOID
//EFIAPI
//ArmGicV3SetControlSystemRegisterEnable (
//  IN UINT32         ControlSystemRegisterEnable
//  );
FUNCTION(ArmGicV3SetControlSystemRegisterEnable)
        msr ICC_SRE_EL1, x0
        ret

//VOID
//ArmGicV3EnableInterruptInterface (
//  VOID
//  );
FUNCTION(ArmGicV3EnableInterruptInterface)
        mov     x0, #1
        msr     ICC_IGRPEN1_EL1, x0
        ret

//VOID
//ArmGicV3DisableInterruptInterface (
//  VOID
//  );
FUNCTION(ArmGicV3DisableInterruptInterface)
        mov     x0, #0
        msr     ICC_IGRPEN1_EL1, x0
        ret

//VOID
//ArmGicV3EndOfInterrupt (
//  IN UINTN          InterruptId
//  );
FUNCTION(ArmGicV3EndOfInterrupt)
        msr     ICC_EOIR1_EL1, x0
        ret

//UINTN
//ArmGicV3AcknowledgeInterrupt (
//  VOID
//  );
FUNCTION(ArmGicV3AcknowledgeInterrupt)
        mrs     x0, ICC_IAR1_EL1
        ret

//VOID
//ArmGicV3SetPriorityMask (
//  IN UINTN          Priority
//  );
FUNCTION(ArmGicV3SetPriorityMask)
        msr     ICC_PMR_EL1, x0
        ret

//VOID
//ArmGicV3SetBinaryPointer (
//  IN UINTN          BinaryPoint
//  );
FUNCTION(ArmGicV3SetBinaryPointer)
        msr     ICC_BPR1_EL1, x0
        ret
