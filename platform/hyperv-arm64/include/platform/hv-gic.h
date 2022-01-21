#pragma once

#include <platform/hv-gen2.h>

// Assuming it's GICv3 yet
#define HV_GICD_ADDRESS (0xFFFF0000)
#define HV_GICR_BASE (0xEFFEE000)
#define HV_GICR_ADDRESS(n) (HV_GICR_BASE + 0x20000 * (n))
