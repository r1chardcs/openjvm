#include <iostream>

#include "common_exception.h"
#include "scriptsdk.h"


int main() {
    const auto script =
        CreateTargetScript(0, "Test", R"(
def check(header):
    if (header["name"] == "my.cheat.class"):
        return True
    return False
)", 100);

    std::cout << script->name << '\n';
    std::cout << script->ring << '\n';
    std::cout << script->type << '\n';
    std::cout << script->source << '\n';
    TargetClassHeader header;
    header.Name = "my.cheat.class";
    header.Size = 100;
    header.FieldsSize = 0;
    header.MethodsSize = 0;
    header.Methods = nullptr;
    header.Fields = nullptr;
    std::cout << ExecuteTargetScript(*script, header) << '\n';
    std::cout << CommonCheckError() << '\n';

}