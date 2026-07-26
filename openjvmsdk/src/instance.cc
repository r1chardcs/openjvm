#define UseConsole

#include "common_exception.h"
#include "windows_helper.h"
#include "runtime_layer.h"
#include "overlay.h"

#include <jvm_inspector.h>

void dllMain(void*) {
    InitializeOverlay();

    if (auto errBuf = CommonCheckError(); errBuf) {
        std::cout << errBuf << '\n';
    }

    InitializeRuntimeLayer();
    if (auto errBuf = CommonCheckError(); errBuf) {
        std::cout << errBuf << '\n';
    }

    CollectDataRuntimeLayer();
    SetRenderable(GetRenderableJvmInspector());

    while (!GetAsyncKeyState(VK_DELETE)) {}
    DestroyOverlay();
}

CREATE_DLL_ENTRY(dllMain)