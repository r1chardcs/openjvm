#ifndef CLASS_ANALYZER
#define CLASS_ANALYZER

#include "runtime_layer.h"

// Проверяет является ли класс примитивным типом:
// int, float, double, long, ...; или массивом этих
// типов
BOOL IsPrimitive(TransitionalClass* transitional_class);


// Проверяет находится ли класс в пакете java.lang
BOOL IsJavaClass(TransitionalClass* transitional_class);

// Проверяет является ли класс сгенерированным JVM
// (для лямбд и другого)
BOOL IsGenerateClass(TransitionalClass* transitional_class);

#endif