#include "scripts.h"
#include "resources/malware_detection.h"

// Тестовый скрипт
PTargetScript GetScriptByID(int ID) {
    return CreateTargetScript(
        0, "Test Script", R"(

def check(header):
    if "?" in header["name"]:
        return True

    return False
)", CHECKING_TYPE_HEADER
    );
}

PTargetScript BaseScript() {
    const auto script
        = reinterpret_cast<const char *>(Resources::malware_detection);

    return CreateTargetScript(0, "Malware Detection", script, CHECKING_TYPE_HEADER);
}
