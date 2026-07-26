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

#undef BOOL
#include <Windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef BOOL(WINAPI* wglSwapBuffers_t)(HDC);
static wglSwapBuffers_t original_wglSwapBuffers = nullptr;
static BOOL WINAPI detour_wglSwapBuffers(HDC unnamedParam1);
static BOOL g_flag_initialize = FALSE;
static HWND g_hwnd = nullptr;
static WNDPROC g_original_wndproc = nullptr;
static LRESULT CALLBACK Overlay_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static Renderable current_renderable;

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
}

void DestroyOverlay() {

    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
    MH_DisableHook(nullptr);
    MH_RemoveHook(nullptr);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

static bool CreateMenuForWindow(HWND hwnd) {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.MouseDrawCursor = false;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init();

    g_hwnd = hwnd;
    g_original_wndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Overlay_WndProc)));

    return TRUE;
}

static LRESULT CALLBACK Overlay_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

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