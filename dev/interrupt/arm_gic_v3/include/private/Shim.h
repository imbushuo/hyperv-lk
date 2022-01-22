// edk2 to LK bridge shim

#ifndef __SHIM_H__
#define __SHIM_H__

#include <lk/bits.h>
#include <lk/debug.h>
#include <lk/err.h>
#include <sys/types.h>

#define EFIAPI
#define IN
#define OUT
#define VOID void
#define UINTN uint64_t
#define UINT32 uint32_t
#define INTN int64_t
#define BOOLEAN uint8_t

#endif