#include "common_memory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common_exception.h"

int CountErrorAllocateMemory = 0;

void CommonErrorAllocateMemory(const U64 Size) {
    printf("Error allocate memory: %llu bytes", Size);
    CountErrorAllocateMemory++;

    if (CountErrorAllocateMemory > 15) {
        CommonSleep(ToSecond(5));
        CommonThrow(ERROR_OUT_OF_MEMORY);
    }
}

PVoid CommonMalloc(const U64 Size) {
    while (true) {
        if (const auto pointer = malloc(Size)) {
            return pointer;
        }

        CommonSleep(ToSecond(1));
        CommonErrorAllocateMemory(Size);
    }
}

PVoid CommonCalloc(U64 Count, U64 BaseSize) {
    return CommonMalloc(Count * BaseSize);
}

PV CommonMemCopy(const PV Dst, const CPV Src, const U64 Size) {
    return memcpy(Dst, Src, Size);
}

U64 CommonCalcString(CPI1 String) {
    return strlen(String);
}

BOOL CommonFree(PVoid Pointer) {
    if (Pointer == nullptr)
        return 0;

    free(Pointer);
    return 1;
}

CPI1 CommonStrDup(const CPI1 Str) {
    if (!Str) return nullptr;

    const size_t len = CommonCalcString(Str);
    const auto copy = static_cast<char*>(CommonMalloc(len + 1));

    CommonMemCopy(copy, Str, len + 1);
    return copy;
}
