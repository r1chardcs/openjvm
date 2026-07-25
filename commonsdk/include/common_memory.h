#ifndef COMMON_MEMORY
#define COMMON_MEMORY

#include "common_typedef.h"

PV CommonMalloc(U64 Size);
PV CommonCalloc(U64 Count, U64 BaseSize);
PV CommonMemCopy(PV Dst, CPV Src, U64 Size);

U64 CommonCalcString(CPI1 String);
BOOL CommonFree(PV Pointer);
CPI1 CommonStrDup(CPI1 Str);

#define StrDup CommonStrDup
#define Deallocate(x) CommonFree(x); x = nullptr;

#endif
