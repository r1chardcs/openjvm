#include "api.h"

#include <Windows.h>
#include <string>

static std::string GetCurrentModuleDirectory() {
    char path[MAX_PATH];
    HMODULE hModule = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCSTR>(&GetCurrentModuleDirectory),
        &hModule);
    GetModuleFileNameA(hModule, path, MAX_PATH);
    std::string full(path);
    size_t pos = full.find_last_of("\\/");
    return (pos != std::string::npos) ? full.substr(0, pos) : std::string();
}

static bool InjectLibrary(HANDLE hProcess, const std::string& dllPath) {
    SIZE_T size = dllPath.size() + 1;
    LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) return false;

    if (!WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), size, nullptr)) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }

    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!loadLibraryAddr) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMem, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);

    return true;
}

Status CheckResources() {
    const std::string dir = GetCurrentModuleDirectory();

    for (std::string name : libraries) {
        if (GetFileAttributesA((dir + "\\sdk\\" + name).c_str()) == INVALID_FILE_ATTRIBUTES)
            return { false, name};
    }

    return {true, "Success" };
}

Status LoadInProcess(int pID) {
    const std::string dir = GetCurrentModuleDirectory();
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, static_cast<DWORD>(pID));
    if (!hProcess)
        return { false, "Failed to open target process" };

    for (std::string name : libraries) {
        auto path = dir + "\\sdk\\" + name;

        if (!InjectLibrary(hProcess, path)) {
            CloseHandle(hProcess);
            return { false, name };
        }
    }

    CloseHandle(hProcess);
    return {true, "Success"};
}
