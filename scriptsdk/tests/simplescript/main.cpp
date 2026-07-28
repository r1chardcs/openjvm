#include <iostream>

#include "common_exception.h"
#include "scriptsdk.h"


int main() {

    InitializeScriptSDK();
    const auto script = CreateTargetScript(0, "Test", R"(
Out_Verbose = "[no log]"

def check(header):
    global Out_Verbose

    if header["name"] == "my.cheat.class":
        Out_Verbose = "Detect my.cheat.class"
        return True

    return False
)", CHECKING_TYPE_HEADER);

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
    auto obj = ExecuteTargetScript(*script, header);
    std::cout << obj.Verbose  << '\n';
    std::cout << (obj.Result? "True" : "False") << '\n';
    std::cout << CommonCheckError() << '\n';

    ShutdownScriptSDK();
}