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
    enum class TargetState {
        INITIALIZING, // Процесс инициализацируется (создает PJVM)
        DATA_COLLECTION, // Собирает данные (переходные классы)
        UNHANDLED_EXCEPTION, // Необработанное исключение
        SUCCESS, // Успешно. Ошибок нет
        NONE
    };

    TargetState State;
    PJVM JVM;
    COMMON_LIST(TransitionalClass, Classes)
}RuntimeLayer;

extern RuntimeLayer RuntimeInstance;

void InitializeRuntimeLayer();
void CollectDataRuntimeLayer();

#endif
