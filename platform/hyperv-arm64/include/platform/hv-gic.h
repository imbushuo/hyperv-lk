#pragma once

#include <platform/hv-gen2.h>

// Assuming it's GICv3 yet

// GICD Physical Address at 0xFFFF0000
#define HV_GICD_ADDRESS (PERIPHERAL_BASE_VIRT + 0x1FFF0000)

// GICR Base at 0xEFFEE000
#define HV_GICR_BASE (PERIPHERAL_BASE_VIRT + 0xFFEE000)
#define HV_GICR_ADDRESS(n) (HV_GICR_BASE + 0x20000 * (n))
