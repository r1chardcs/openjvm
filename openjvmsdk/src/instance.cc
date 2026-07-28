#define UseConsole
#include "common_exception.h"
#include "windows_helper.h"
#include "runtime_layer.h"
#include "overlay.h"

#include <jvm_inspector.h>

#include "class_analyzer.h"
#include "common_memory.h"
#include "scripts.h"
#include "../../scriptsdk/thirdparty/py/pocketpy.h"

PTargetScript Out_Update_Script = nullptr;

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

    AddRenderable(GetRenderableJvmInspector());
    AddRenderable(GetRenderableUnhookMenu());
    AddRenderable(GetRenderableScriptMenu());

    for (int i = 0; i < RuntimeInstance.ClassesSize; i++) {
        // Оптимизация: я думаю нет смысла проверять
        // примитивные, сгенерированные и jdk классы.
        // Хоть сгенерированные классы и могут быть вредоносными,
        // я считаю что их можно пропустить.
        if (IsGenerateClass(&RuntimeInstance.Classes[i])) continue;
        if (IsPrimitive(&RuntimeInstance.Classes[i])) continue;
        if (IsJavaClass(&RuntimeInstance.Classes[i])) continue;

        RuntimeInstance.Classes[i].Check = CheckBaseTarget(RuntimeInstance.Classes[i]);

        if (RuntimeInstance.Classes[i].Check)
            RuntimeInstance.Classes[i].Owner = MALWARE_DETECT_OWNER;

        if (const auto errBuf = CommonCheckError(); errBuf) {
            printf("unhandled exception when checking the class %s, %s",
                RuntimeInstance.Classes[i].Name, errBuf);
        }
    }
    while (TRUE) {
        if (!Out_Update_Script) {
            UpdateRuntimeLayer();
            CommonSleep(100/*ms*/);
        }
        else {
            for (int i = 0; i < RuntimeInstance.ClassesSize; i++) {
                // Не проверяем классы, проверяенные статическими методами анализа,
                // например: malware_detect.py
                if (RuntimeInstance.Classes[i].Owner
                            && strcmp(RuntimeInstance.Classes[i].Owner, MALWARE_DETECT_OWNER) == 0)
                    continue;

                if (IsGenerateClass(&RuntimeInstance.Classes[i])) continue;
                if (IsPrimitive(&RuntimeInstance.Classes[i])) continue;
                if (IsJavaClass(&RuntimeInstance.Classes[i])) continue;

                RuntimeInstance.Classes[i].Check = CheckClassTarget(RuntimeInstance.Classes[i], Out_Update_Script, false);
                if (RuntimeInstance.Classes[i].Check) {
                    if (RuntimeInstance.Classes->Owner)
                        CommonFree(const_cast<PV>(
                            reinterpret_cast<CPV>(RuntimeInstance.Classes->Owner)));

                    RuntimeInstance.Classes->Owner = StrDup(Out_Update_Script->name);
                }
                if (const auto errBuf = CommonCheckError(); errBuf) {
                    printf("unhandled exception when checking the class %s, %s",
                        RuntimeInstance.Classes[i].Name, errBuf);
                }
            }
            DestroyTargetScript(Out_Update_Script);
            Out_Update_Script = nullptr;
        }
    }
}

CREATE_DLL_ENTRY(dllMain)