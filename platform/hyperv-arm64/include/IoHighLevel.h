#ifndef __IO_HIGH_LEVEL_H__
#define __IO_HIGH_LEVEL_H__

#include <lk/reg.h>

#define MmioOr32(Address, OrData) writel(readl((Address)) | (OrData), (Address))
#define MmioRead32(Address) readl((Address))
#define MmioWrite32(Address, Data) writel((Data), (Address))
#define MmioAndThenOr32(Address, AndData, OrData)                              \
  MmioWrite32((Address), (MmioRead32((Address)) & (AndData)) | (OrData))

#define MmioRead64(Address) (*REG64(Address))
#define MmioWrite64(Address, Data) (*REG64(Address) = (Data))

#endif
