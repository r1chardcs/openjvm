#define UseConsole
#include "common_exception.h"
#include "windows_helper.h"
#include "overlay.h"

void dllMain(void*) {
    InitializeOverlay();
    if (auto errBuf = CommonCheckError(); errBuf) {
        std::cout << errBuf << '\n';
    }

    while (!GetAsyncKeyState(VK_DELETE)) {}
    DestroyOverlay();
}

CREATE_DLL_ENTRY(dllMain)