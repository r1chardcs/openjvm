#define TYPEDEF_BOOL
#include <overlay.h>
#include <gl/GL.h>
#include <MinHook/MinHook.h>
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_opengl3.h"
#include "ImGui/imgui_impl_win32.h"
#include <common_exception.h>
#include <common_system.h>
#include <cstdio>
#include <iostream>
#include <resources/Roboto_Medium.h>
#include <Windows.h>
#include <atomic>
#include <thread>
#include <chrono>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern void CreateBaseImGuiStyle(ImGuiIO io);

typedef BOOL(WINAPI* wglSwapBuffers_t)(HDC);
static wglSwapBuffers_t original_wglSwapBuffers = nullptr;
static BOOL WINAPI detour_wglSwapBuffers(HDC unnamedParam1);
static BOOL g_flag_initialize = FALSE;
static HWND g_hwnd = nullptr;
static WNDPROC g_original_wndproc = nullptr;
static LRESULT CALLBACK Overlay_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static Renderable current_renderable;

static std::atomic<bool> g_hookCalled{false};
static std::atomic<bool> g_fallbackActive{false};
static std::atomic<bool> g_fallbackShouldStop{false};
static std::thread g_watchdogThread;
static HWND g_fallbackHwnd = nullptr;

static ImGuiContext* g_hookImGuiContext = nullptr;
static ImGuiContext* g_fallbackImGuiContext = nullptr;

static void WatchdogThreadProc();
static void RunFallbackWindow();
static LRESULT CALLBACK Fallback_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void SetRenderable(Renderable renderable) {
    current_renderable = renderable;
}

void InitializeOverlay() {
    if (MH_Initialize() != MH_OK) {
        Throw("Error initialize Hook's");
    }
    auto wglSwapBuffer = CommonFindSymbolEx("OpenGL32.dll", "wglSwapBuffers");

    if (const auto out_error = CommonCheckError(); out_error) {
        Throw(out_error);
    }

    if (MH_CreateHook((void*)wglSwapBuffer,
            reinterpret_cast<void*>(&detour_wglSwapBuffers),
            reinterpret_cast<void**>(&original_wglSwapBuffers)) != MH_OK) {
        Throw("Error Hook wglSwapBuffer.");
    }

    if (MH_EnableHook((void*)wglSwapBuffer) != MH_OK) {
        Throw("Error Hook Enable: wglSwapBuffer.");
    }

    g_hookCalled.store(false, std::memory_order_relaxed);
    g_watchdogThread = std::thread(WatchdogThreadProc);
    g_watchdogThread.detach();
}

void DestroyOverlay() {
    g_fallbackShouldStop.store(true, std::memory_order_relaxed);

    if (g_hwnd) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
    }
    MH_DisableHook(nullptr);
    MH_RemoveHook(nullptr);

    if (g_hookImGuiContext && !g_fallbackActive.load(std::memory_order_relaxed)) {
        ImGui::SetCurrentContext(g_hookImGuiContext);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(g_hookImGuiContext);
        g_hookImGuiContext = nullptr;
    }
}

static bool CreateMenuForWindow(HWND hwnd) {
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    g_hookImGuiContext = ctx;

    ImGuiIO& io = ImGui::GetIO();

    io.MouseDrawCursor = false;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    static const ImWchar ranges[] =
    {
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0x2DE0, 0x2DFF,
        0xA640, 0xA69F,
        0,
    };

    ImFontConfig cfg;
    cfg.GlyphRanges = ranges;
    cfg.FontDataOwnedByAtlas = false;

    io.Fonts->AddFontFromMemoryTTF((void*)Resources::Roboto_Medium, Resources::Roboto_Medium_size, 18, &cfg);

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init();

    g_hwnd = hwnd;
    g_original_wndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Overlay_WndProc)));

    return TRUE;
}

static LRESULT CALLBACK Overlay_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (g_hookImGuiContext) {
        ImGui::SetCurrentContext(g_hookImGuiContext);
    }

    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

    ImGuiIO& io = ImGui::GetIO();
    bool block_mouse = io.WantCaptureMouse &&
        (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
         msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ||
         msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP ||
         msg == WM_MOUSEWHEEL || msg == WM_MOUSEMOVE ||
         msg == WM_MOUSEHWHEEL);
    bool block_keyboard = io.WantCaptureKeyboard &&
        (msg == WM_KEYDOWN || msg == WM_KEYUP ||
         msg == WM_CHAR || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP);

    if (block_mouse || block_keyboard)
        return TRUE;

    return CallWindowProcW(g_original_wndproc, hwnd, msg, wparam, lparam);
}

static BOOL WINAPI detour_wglSwapBuffers(HDC unnamedParam1) {
    g_hookCalled.store(true, std::memory_order_relaxed);

    HWND hwnd = WindowFromDC(unnamedParam1);

    HGLRC origin_context{ wglGetCurrentContext() };
    static HGLRC new_context{};

    if (static bool was_init{}; was_init == false)
    {
        new_context = wglCreateContext(unnamedParam1);
        wglMakeCurrent(unnamedParam1, new_context);

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glViewport(0, 0, viewport[2], viewport[3]);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, viewport[2], viewport[3], 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);

        CreateMenuForWindow(hwnd);
        was_init = true;
    }
    else
    {
        wglMakeCurrent(unnamedParam1, new_context);

        ImGui::SetCurrentContext(g_hookImGuiContext);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        CreateBaseImGuiStyle(ImGui::GetIO());
        if (current_renderable.callback) {
            current_renderable.callback({});
        }

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    wglMakeCurrent(unnamedParam1, origin_context);
    return original_wglSwapBuffers(unnamedParam1);
}

static void WatchdogThreadProc() {
    std::this_thread::sleep_for(std::chrono::seconds(3));

    if (!g_hookCalled.load(std::memory_order_relaxed)) {
        RunFallbackWindow();
    }
}

static LRESULT CALLBACK Fallback_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (g_fallbackImGuiContext) {
        ImGui::SetCurrentContext(g_fallbackImGuiContext);
    }

    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void RunFallbackWindow() {
    g_fallbackActive.store(true, std::memory_order_relaxed);

    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Fallback_WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = L"OverlayFallbackWindowClass";
    RegisterClassExW(&wc);

    g_fallbackHwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Overlay",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        900, 700,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_fallbackHwnd) {
        g_fallbackActive.store(false, std::memory_order_relaxed);
        return;
    }

    HDC hdc = GetDC(g_fallbackHwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(hdc, pixelFormat, &pfd)) {
        ReleaseDC(g_fallbackHwnd, hdc);
        DestroyWindow(g_fallbackHwnd);
        g_fallbackHwnd = nullptr;
        g_fallbackActive.store(false, std::memory_order_relaxed);
        return;
    }

    HGLRC glContext = wglCreateContext(hdc);
    if (!glContext) {
        ReleaseDC(g_fallbackHwnd, hdc);
        DestroyWindow(g_fallbackHwnd);
        g_fallbackHwnd = nullptr;
        g_fallbackActive.store(false, std::memory_order_relaxed);
        return;
    }

    wglMakeCurrent(hdc, glContext);

    ShowWindow(g_fallbackHwnd, SW_SHOW);
    UpdateWindow(g_fallbackHwnd);

    ImGuiContext* fallbackCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext(fallbackCtx);
    g_fallbackImGuiContext = fallbackCtx;

    ImGuiIO& io = ImGui::GetIO();

    io.MouseDrawCursor = false;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    static const ImWchar ranges[] =
    {
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0x2DE0, 0x2DFF,
        0xA640, 0xA69F,
        0,
    };

    ImFontConfig cfg;
    cfg.GlyphRanges = ranges;
    cfg.FontDataOwnedByAtlas = false;

    io.Fonts->AddFontFromMemoryTTF((void*)Resources::Roboto_Medium, Resources::Roboto_Medium_size, 18, &cfg);

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_fallbackHwnd);
    ImGui_ImplOpenGL3_Init();

    MSG msg = {};
    bool running = true;

    while (running && !g_fallbackShouldStop.load(std::memory_order_relaxed)) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!running) break;

        ImGui::SetCurrentContext(fallbackCtx);

        RECT rect;
        GetClientRect(g_fallbackHwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        CreateBaseImGuiStyle(ImGui::GetIO());
        if (current_renderable.callback) {
            current_renderable.callback({});
        }

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(hdc);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    ImGui::SetCurrentContext(fallbackCtx);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(fallbackCtx);
    g_fallbackImGuiContext = nullptr;

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(glContext);
    ReleaseDC(g_fallbackHwnd, hdc);
    DestroyWindow(g_fallbackHwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);

    g_fallbackHwnd = nullptr;
    g_fallbackActive.store(false, std::memory_order_relaxed);
}