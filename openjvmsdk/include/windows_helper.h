#ifndef WINDOWS_HELPER
#define WINDOWS_HELPER

#include <Windows.h>

#ifdef UseConsole
#include <iostream>
#define AllocConsoleBegin \
AllocConsole(); \
freopen("CONIN$", "r", stdin); \
freopen("CONOUT$", "w", stdout); \
freopen("CONOUT$", "w", stderr); \
std::cout.clear(); \
std::cin.clear();

#define AllocConsoleEnd \
fclose(stdin); \
fclose(stdout); \
fclose(stderr); \
FreeConsole();
#else
#define AllocConsoleBegin
#define AllocConsoleEnd
#endif

#define SetDllMain(constructor) \
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) { \
switch (ul_reason_for_call) { \
case DLL_PROCESS_ATTACH: \
CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)constructor, hModule, 0, NULL); \
break; \
case DLL_PROCESS_DETACH: \
default: \
break; \
} \
return TRUE; \
}

#define CREATE_DLL_ENTRY(constructor) \
void __load_program(HMODULE __module) { \
AllocConsoleBegin \
constructor(__module); \
AllocConsoleEnd; \
FreeLibraryAndExitThread(__module, 0); \
} \
SetDllMain(__load_program)

#endif