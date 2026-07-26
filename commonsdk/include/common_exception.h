#ifndef COMMON_EXCEPTION_H
#define COMMON_EXCEPTION_H

#include "common_typedef.h"

#define ToSecond(x) ((x) * 1000)

#define ERROR_OUT_OF_MEMORY "Out Of Memory"
#define ERROR_LOGIC_ERROR   "Logic Error"

#ifdef SUPPORT_THROW
    #ifdef Throw
        #undef Throw
    #endif
#endif

#ifndef Throw
    #define Throw(x) CommonThrow(x); return
    #define _Throw(x) CommonThrow(x); return
#endif

#define Catch(x) if (const auto x = CommonCheckError(); x)

#define _FORMAT(fmt, ...) CommonFormatImpl(fmt, ##__VA_ARGS__)

typedef void(*ThrowCallbackImpl)(CPI1);

void CommonThrow(const char* Msg);
CPI1 CommonCheckError();
PI1 CommonFormatImpl(const char* Fmt, ...);
void CommonIfErrorAbort();
void CommonSleep(U64 Milliseconds);

extern CPI1 Out_Throw_Buf;
extern ThrowCallbackImpl Out_Throw_Callback;

#endif
