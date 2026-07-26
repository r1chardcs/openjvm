#include <common_system.h>
#include <common_exception.h>

HMODULE CommonFindModuleA(const char *Name) {
    return GetModuleHandleA(Name);
}

FARPROC CommonFindSymbolA(HMODULE Module, const char *Name) {
    if (!Module) {
        Throw(_FORMAT("Invalid Module for: %s", Name)) nullptr;
    }

    return GetProcAddress(Module, Name);
}

FARPROC CommonFindSymbolEx(const char *Module, const char *Name) {
    const HMODULE hModule = CommonFindModuleA(Module);
    if (!hModule) {
        Throw(_FORMAT("Invalid Module: %s (%s)", Module, Name)) nullptr;
    }

    const FARPROC fSymbol = CommonFindSymbolA(hModule, Name);
    if (!fSymbol) {
        Throw(_FORMAT("Invalid Symbol: %s (%s)", Module, Name)) nullptr;
    }

    return fSymbol;
}
