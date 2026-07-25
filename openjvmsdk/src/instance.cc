#define UseConsole
#include "windows_helper.h"

void dllMain() {
    while (!GetAsyncKeyState(VK_DELETE)) {

    }
}

SetDllMain(dllMain)