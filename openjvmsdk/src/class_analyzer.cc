#include "class_analyzer.h"

#include <string>

BOOL IsPrimitive(TransitionalClass *transitional_class) {
    if (!transitional_class) return false;
    const std::string className = transitional_class->name;

    return className.find("[") != std::string::npos ||
                             className == "int" || className == "long" ||
                             className == "float" || className == "double" ||
                             className == "boolean" || className == "byte" ||
                             className == "char" || className == "short" ||
                             className == "void";
}

BOOL IsJavaClass(TransitionalClass *transitional_class) {
    if (!transitional_class) return false;
    const std::string className = transitional_class->name;
    return className.find("java/lang") != std::string::npos;
}

BOOL IsGenerateClass(TransitionalClass *transitional_class) {
    if (!transitional_class) return false;
    const std::string className = transitional_class->name;

    return className.find("$") != std::string::npos;
}
