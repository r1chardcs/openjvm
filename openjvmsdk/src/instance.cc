#define UseConsole

#include "common_exception.h"
#include "windows_helper.h"
#include "runtime_layer.h"
#include "overlay.h"

#include <jvm_inspector.h>

#include "class_analyzer.h"
#include "../../scriptsdk/thirdparty/py/pocketpy.h"

void dllMain(void*/*handle*/) {
    py_initialize();
    Out_Throw_Callback = GlobalErrorCallback;

    InitializeOverlay();
    Catch(errBuf) {
        printf("unhandled exception when 'Initialize Overlay'\n");
        _Throw (errBuf);
    }

    InitializeRuntimeLayer();
    Catch(errBuf) {
        printf("unhandled exception when 'Initialize Runtime Layer'\n");
        _Throw (errBuf);
    }

    CollectDataRuntimeLayer();
    Catch(errBuf) {
        printf("unhandled exception when 'Collect Data'\n");
        _Throw (errBuf);
    }

    SetRenderable(GetRenderableJvmInspector());
    for (int i = 0; i < RuntimeInstance.ClassesSize; i++) {
        // Оптимизация: я думаю нет смысла проверять
        // примитивные, сгенерированные и jdk классы.
        // Хоть сгенерированные классы и могут быть вредоносными,
        // я считаю что их можно пропустить.
        if (IsGenerateClass(&RuntimeInstance.Classes[i])) continue;
        if (IsPrimitive(&RuntimeInstance.Classes[i])) continue;
        if (IsJavaClass(&RuntimeInstance.Classes[i])) continue;

        RuntimeInstance.Classes[i].Check = CheckBaseTarget(RuntimeInstance.Classes[i]);
        if (const auto errBuf = CommonCheckError(); errBuf) {
            printf("unhandled exception when checking the class %s, %s",
                RuntimeInstance.Classes[i].name, errBuf);
        }
    }
    while (TRUE) {
        UpdateRuntimeLayer();
        CommonSleep(100/*ms*/);
    }

    py_finalize();
    DestroyOverlay();
}

CREATE_DLL_ENTRY(dllMain)