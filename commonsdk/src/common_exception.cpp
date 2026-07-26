#include <common_exception.h>
#include <common_memory.h>
#include <cstdio>
#include <Windows.h>

CPI1 Out_Throw_Buf = nullptr;

PI1 CommonFormatImpl(const char *Fmt, ...) {
    va_list args;
    va_start(args, Fmt);

    va_list argsCopy;
    va_copy(argsCopy, args);
    const int size = vsnprintf(nullptr, 0, Fmt, argsCopy);
    va_end(argsCopy);

    if (size < 0) {
        va_end(args);
        CommonThrow("vsnprintf size calculation failed.");
        return nullptr;
    }

    const auto buffer = static_cast<char*>(CommonMalloc(size + 1));
    if (!buffer) {
        va_end(args);
        CommonThrow("Failed to allocate format buffer.");
        return nullptr;
    }

    vsnprintf(buffer, size + 1, Fmt, args);
    va_end(args);

    return buffer;
}

void CommonThrow(const char *Msg) {
    printf("Throw %s\n", Msg);
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