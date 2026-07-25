#include <common_exception.h>
#include <common_memory.h>
#include <cstdio>
#include <Windows.h>

CPI1 Out_Throw_Buf = nullptr;

void CommonThrow(const char *Msg) {
    if (Out_Throw_Buf) {
        CommonIfErrorAbort();
        return;
    }

    if (!Msg) {
        Out_Throw_Buf = "<null exception>";
        return;
    }

    const auto size = CommonCalcString(Msg) + 1;
    Out_Throw_Buf = static_cast<CPI1>(CommonMalloc(size));

    if (!Out_Throw_Buf) {
        Out_Throw_Buf = "<out of memory>";
        return;
    }

    CommonMemCopy((void*)Out_Throw_Buf, Msg, size);
}

CPI1 CommonCheckError() {
    if (Out_Throw_Buf == nullptr)
        return nullptr;

    const auto size = CommonCalcString(Out_Throw_Buf) + 1;
    const auto newBuffer = static_cast<CPI1>(CommonMalloc(size));

    CommonMemCopy((void*)newBuffer, Out_Throw_Buf, size);
    Out_Throw_Buf = nullptr;
    return newBuffer;
}

void CommonIfErrorAbort() {
    if (!Out_Throw_Buf) return;

    printf("Exception: %s\n", Out_Throw_Buf);
    ExitProcess(-1);
}

void CommonSleep(const U64 Milliseconds) {
    Sleep(Milliseconds);
}