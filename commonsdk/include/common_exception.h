#ifndef COMMON_EXCEPTION_H
#define COMMON_EXCEPTION_H

#include "common_typedef.h"

#define ToSecond(x) ((x) * 1000)
#define ERROR_OUT_OF_MEMORY "Out Of Memory"

void CommonThrow(const char* Msg);
CPI1 CommonCheckError();
void CommonIfErrorAbort();
void CommonSleep(U64 Milliseconds);

extern CPI1 Out_Throw_Buf;

#endif
