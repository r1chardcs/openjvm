#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <gl/GL.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "ImGui/imgui_impl_win32.h"
#include "api.h"
#include "resources/resources.h"

#pragma comment(lib, "opengl32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
extern void CreateBaseImGuiStyle(ImGuiIO io);

struct ProcessEntry {
    DWORD pid;
    std::string name;
    std::string title;
    bool loading;
    bool loaded;
    std::string statusMsg;
};

static std::vector<ProcessEntry> g_processes;
static bool g_needRefresh = true;

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    auto* map = reinterpret_cast<std::map<DWORD, std::string>*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!IsWindowVisible(hwnd)) return TRUE;
    int len = GetWindowTextLengthA(hwnd);
    if (len == 0) return TRUE;
    if (map->find(pid) != map->end()) return TRUE;
    std::string title(len, '\0');
    GetWindowTextA(hwnd, &title[0], len + 1);
    title.resize(len);
    (*map)[pid] = title;
    return TRUE;
}

static void RefreshProcessList() {
    std::map<DWORD, std::string> titles;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&titles));

    std::vector<ProcessEntry> updated;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &entry)) {
        do {
            std::string exe(entry.szExeFile);
            std::string lower = exe;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            if (lower == "java.exe" || lower == "javaw.exe") {
                ProcessEntry pe;
                pe.pid = entry.th32ProcessID;
                pe.name = exe;
                auto it = titles.find(pe.pid);
                pe.title = (it != titles.end()) ? it->second : "";
                pe.loading = false;
                pe.loaded = false;
                pe.statusMsg.clear();

                for (auto& old : g_processes) {
                    if (old.pid == pe.pid) {
                        pe.loaded = old.loaded;
                        pe.statusMsg = old.statusMsg;
                        break;
                    }
                }
                updated.push_back(pe);
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    g_processes = updated;
    g_needRefresh = false;
}

static void DrawCard(ProcessEntry& proc) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 78.0f);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    ImU32 bgColor = proc.loaded ?
        ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, 0.95f)) :
        ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 0.92f));

    ImU32 borderColor = proc.loaded ?
        ImGui::GetColorU32(ImVec4(0.3f, 0.7f, 0.3f, 0.6f)) :
        ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.5f));

    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor, 3.0f);
    draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), borderColor, 3.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 14, pos.y + 10));
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", proc.name.c_str());
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 14, pos.y + 34));
    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "PID %d", proc.pid);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 14, pos.y + 54));
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.9f), "%s", proc.title.c_str());

    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - 84, pos.y + 26));
    ImGui::PushID(static_cast<int>(proc.pid));

    if (proc.loaded) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.25f, 0.5f));
        ImGui::Button("Done", ImVec2(64, 26));
        ImGui::PopStyleColor(3);
    }
    else if (proc.loading) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        ImGui::Button("Wait", ImVec2(64, 26));
        ImGui::PopStyleColor(3);
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.18f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

        if (ImGui::Button("Load", ImVec2(64, 26))) {
            proc.loading = true;
            Status resStatus = CheckResources();
            if (!resStatus.success) {
                proc.loading = false;
                proc.statusMsg = resStatus.msg;
            }
            else {
                Status loadStatus = LoadInProcess(static_cast<int>(proc.pid));
                proc.loading = false;
                proc.loaded = loadStatus.success;
                proc.statusMsg = loadStatus.msg;
            }
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::PopID();
    ImGui::Dummy(ImVec2(size.x, 4.0f));
}

static void DrawGui(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(
        static_cast<float>(rect.right - rect.left),
        static_cast<float>(rect.bottom - rect.top)));

    ImGui::Begin("Main", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);


    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 76);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.18f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));

    if (ImGui::Button("Reset", ImVec2(-1, 26))) {
        g_needRefresh = true;
    }

    ImGui::PopStyleColor(3);

    int total = (int)g_processes.size();
    int loaded = 0;
    for (auto& p : g_processes) if (p.loaded) loaded++;

    ImGui::SetCursorPosX(14);
    ImGui::TextColored(ImVec4(0.35f, 0.35f, 0.35f, 1.0f), "%d total · %d loaded", total, loaded);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

    ImGui::BeginChild("##list", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground);

    if (g_needRefresh) {
        RefreshProcessList();
    }

    if (g_processes.empty()) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 60);
        ImGui::TextColored(ImVec4(0.25f, 0.25f, 0.25f, 1.0f), "No Java processes");
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 75);
        ImGui::TextColored(ImVec4(0.18f, 0.18f, 0.18f, 0.8f), "Start Java app to see it");
    }
    else {
        for (auto& proc : g_processes) {
            DrawCard(proc);
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

static HGLRC g_glContext = nullptr;
static HDC g_hdc = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_glContext)
            glViewport(0, 0, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int main() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0, 0,
        GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"OpenJVM", nullptr };
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"OpenJVM Launcher",
        WS_OVERLAPPEDWINDOW, 100, 100, 380, 420,
        nullptr, nullptr, wc.hInstance, nullptr);

    g_hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pf = ChoosePixelFormat(g_hdc, &pfd);
    SetPixelFormat(g_hdc, pf, &pfd);

    g_glContext = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_glContext);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImFontConfig baseCfg;
    baseCfg.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)Resources::Roboto_Medium, Resources::Roboto_Bold_size, 16, &baseCfg);
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    RefreshProcessList();

    bool running = true;
    ULONGLONG lastRefresh = GetTickCount64();

    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        ULONGLONG now = GetTickCount64();
        if (now - lastRefresh > 2000) {
            g_needRefresh = true;
            lastRefresh = now;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        CreateBaseImGuiStyle(ImGui::GetIO());
        DrawGui(hwnd);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.04f, 0.04f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(g_hdc);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(g_glContext);
    ReleaseDC(hwnd, g_hdc);
    DestroyWindow(hwnd);

    return 0;
}