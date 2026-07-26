#ifndef RUNTIME_LAYER
#define RUNTIME_LAYER

#include <common_typedef.h>
#undef Throw

#include <jni_helper.h>

typedef struct {
    CPI1 name;
    CPI1 signature;
}TransitionalField;

typedef struct {
    CPI1 name;
    CPI1 signature;
}TransitionalMethod;

typedef struct {
    CPI1 name;
    COMMON_LIST(TransitionalField, Fields);
    COMMON_LIST(TransitionalMethod, Methods);
}TransitionalClass;

typedef struct {
    enum class TargetAction {
        INITIALIZE, // Инициализация JVM (JNI)
        DATA_COLLECTED, // Сбор данных (Классов в райнтайме)
        DEALLOCATE_DATA, // Очистка памяти (Классов в райнтме)
        NONE // Ничего не делаем
    };

    TargetAction PreAction1 = TargetAction::NONE;
    TargetAction PreAction2 = TargetAction::NONE;
    TargetAction Action = TargetAction::NONE;

    PJVM JVM;
    COMMON_LIST(TransitionalClass, Classes)
}RuntimeLayer;

extern RuntimeLayer RuntimeInstance;

BOOL SetActionRuntimeLayer(RuntimeLayer::TargetAction action);
void UpdateRuntimeLayer();
void InitializeRuntimeLayer();
void CollectDataRuntimeLayer();
void DeallocateRuntimeLayer();

#endif
