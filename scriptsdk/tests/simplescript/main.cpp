#include <iostream>

#include "common_exception.h"
#include "scriptsdk.h"


int main() {
    auto script =
        CreateTargetScript(0, "Test", R"()", 100);

    std::cout << script->name << '\n';
    std::cout << script->ring << '\n';
    std::cout << script->type << '\n';
    TargetClassHeader header;
    header.Name = "my.cheat.class";
    header.Size = 100;
    header.FieldsSize = 0;
    header.MethodsSize = 0;
    header.Methods = nullptr;
    header.Fields = nullptr;
    ExecuteTargetScript(*script, header);
    std::cout << CommonCheckError() << '\n';

}