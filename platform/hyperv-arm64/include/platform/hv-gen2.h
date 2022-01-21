/*
 * Copyright (c) 2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

/*  Due to the current limitation, limit how much memory we can use.
    Memory map dumped from SV2 machine:

    Hyper-V arm64 Full System Memory Map, left inclusive and right exclusive:
    0000000000000000 - 00000000E0000000 System Memory
    00000000E0000000 - 00000000EFFFFFFF System Memory
    00000000EFFFFFFF - 0000000100000000 Low MMIO
    0000000100000000 - 0000000111200000 System Memory
    0000000111200000 - 00000009FFE00000 System Memory
    00000009FFE00000 - 0000001000000000 High MMIO
    0000001000000000 - 0000010000000000 System Memory
    0000010000000000 - 0000020000000000 System Memory
    0000020000000000 - 0000040000000000 System Memory
    0000040000000000 - 0000080000000000 System Memory
    0000080000000000 - 0000100000000000 System Memory
    0000100000000000 - 0000200000000000 System Memory
    0000200000000000 - 0000400000000000 System Memory
    0000400000000000 - 0000800000000000 System Memory
    0000800000000000 - 0001000000000000 System Memory

    256TB spae total, which means 48bits of address space possible
*/

#define MEMORY_BASE_PHYS (0)
#if ARCH_ARM64
// 3584MB
#define MEMORY_APERTURE_SIZE 0xE0000000
#else
#error "Only ARM64 is supported"
#endif

/* Hyper-V has 256MB MMIO */
#define PERIPHERAL_BASE_PHYS (0xE0000000)
#define PERIPHERAL_BASE_SIZE (0x10000000UL) // 256MB

#if ARCH_ARM64
#define PERIPHERAL_BASE_VIRT (0xffffffffc0000000ULL) // -1GB
#else
#error "Only ARM64 is supported"
#endif

/* individual peripherals in this mapping */
#define UART0_BASE (PERIPHERAL_BASE_VIRT + 0xFFEC000)
#define UART0_SIZE (0x00001000)

// Alias UART0 to UART
#define UART_BASE UART0_BASE
#define UART_SIZE UART0_SIZE

#define UART1_BASE (PERIPHERAL_BASE_VIRT + 0xFFEB000)
#define UART1_SIZE (0x00001000)

/* interrupts */
#define ARM_GENERIC_TIMER_PHYSICAL_INT 19
#define ARM_GENERIC_TIMER_VIRTUAL_INT 20
#define UART0_INT (32 + 1)
#define UART1_INT (32 + 2)

#define MAX_INT 128
