#ifndef RUNTIME_LAYER
#define RUNTIME_LAYER

#include <common_typedef.h>
#undef Throw // для jniEnv->Throw()

#include <jni_helper.h>
#include "scriptsdk.h"

typedef struct {
    CPI1 name;
    CPI1 signature;
}TransitionalField;

typedef struct {
    CPI1 name;
    CPI1 signature;
}TransitionalMethod;

typedef struct {
    BOOL Check;
    CPI1 Owner; // Verbose
    CPI1 Name;
    COMMON_LIST(TransitionalField, Fields);
    COMMON_LIST(TransitionalMethod, Methods);
}TransitionalClass;

void GlobalErrorCallback(const char* Msg);
TargetClassHeader ToTargetClassHeader(const TransitionalClass &klass);
TargetExecuteResult CheckBaseTarget(const TransitionalClass &transitional_class);
TargetExecuteResult CheckClassTarget(const TransitionalClass &transitional_class, PTargetScript script, bool free_memory);

typedef struct {
    enum class TargetAction {
        INITIALIZE, // Инициализация JVM (JNI)
        DATA_COLLECTED, // Сбор данных (Классов в райнтайме)
        DEALLOCATE_DATA, // Очистка памяти (Классов в райнтме)
        REFRESH_DATA_COLLECTED, // Пересбор данных. Очистка - Сбор. (Классы райнтайм)
        NONE // Ничего не делаем
    };

    TargetAction Action = TargetAction::NONE;

    PJVM JVM;
    COMMON_LIST(TransitionalClass, Classes)
}RuntimeLayer;

extern RuntimeLayer RuntimeInstance;

BOOL SetActionRuntimeLayer(RuntimeLayer::TargetAction action);
void UpdateRuntimeLayer();

// Методы только для вызова в основном потоке dllMain,
// для взаимодействия из вне (или другого потока) нужно испоьзовать
// <>SetActionRuntimeLayer
void InitializeRuntimeLayer();
void CollectDataRuntimeLayer();
void DeallocateRuntimeLayer();

void OnUnhandledError(const char* Msg);

// Владелец обнаруженный malware_detect.py
// и/или основной обработчик не валидных классов
#define MALWARE_DETECT_OWNER "Invalid Class"

#endif
