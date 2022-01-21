#pragma once

#include <platform/hv-gen2.h>

// Assuming it's GICv3 yet
#define HV_GICD_ADDRESS (0xFFFF0000)
#define HV_GICR_ADDRESS(n) (0xEFFEE000 + 0x20000 * (n))
