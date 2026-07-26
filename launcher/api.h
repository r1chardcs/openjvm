#ifndef LAUNCHER_API
#define LAUNCHER_API

#include <string>
#include <vector>

inline std::vector<std::string> libraries {
    "libcommonsdk.dll",
    "libscriptsdk.dll",
    "libopenjvmsdk.dll",
};

struct Status {
    bool success;
    std::string msg;
};

Status CheckResources();
Status LoadInProcess(int pID);

#endif
