/** @file
  Macros to work around lack of Clang support for LDR register, =expr

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Portions copyright (c) 2011 - 2014, ARM Ltd. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ASM_MACRO_IO_LIBV8_H_
#define ASM_MACRO_IO_LIBV8_H_

#define MOV32(Reg, Val)                                                        \
  movz Reg, (Val) >> 16, lsl #16;                                              \
  movk Reg, (Val)&0xffff

#define MOV64(Reg, Val)                                                        \
  movz Reg, (Val) >> 48, lsl #48;                                              \
  movk Reg, ((Val) >> 32) & 0xffff, lsl #32;                                   \
  movk Reg, ((Val) >> 16) & 0xffff, lsl #16;                                   \
  movk Reg, (Val)&0xffff

#endif // ASM_MACRO_IO_LIBV8_H_
