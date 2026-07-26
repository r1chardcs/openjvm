#define UseConsole

#include "common_exception.h"
#include "windows_helper.h"
#include "runtime_layer.h"
#include "overlay.h"

#include <jvm_inspector.h>

void dllMain(void*) {
    Out_Throw_Callback = GlobalErrorCallback;
    InitializeOverlay();

    if (const auto errBuf = CommonCheckError(); errBuf) {
        std::cout << errBuf << '\n';
    }

    InitializeRuntimeLayer();
    if (const auto errBuf = CommonCheckError(); errBuf) {
        std::cout << errBuf << '\n';
    }

    CollectDataRuntimeLayer();
    SetRenderable(GetRenderableJvmInspector());

    while (!GetAsyncKeyState(VK_DELETE)) {
        UpdateRuntimeLayer();
    }
    DestroyOverlay();
}

CREATE_DLL_ENTRY(dllMain)