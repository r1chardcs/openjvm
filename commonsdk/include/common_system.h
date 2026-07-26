#ifndef COMMON_SYSTEM
#define COMMON_SYSTEM

#include <Windows.h>

HMODULE CommonFindModuleA(const char* Name);
FARPROC CommonFindSymbolA(HMODULE Module,
                          const char* Name);

FARPROC CommonFindSymbolEx(const char* Module,
                           const char* Name);

#endif
