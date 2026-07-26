#define TYPEDEF_BOOL // Нужно чтобы не
//  было конфликта между разными BOOL
//        из WinApi и <common_typedef.h>

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
static BOOL g_flag_initialize = false;
static HWND g_hwnd = nullptr;
static WNDPROC g_original_wndproc = nullptr;
static LRESULT CALLBACK Overlay_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void InitializeOverlay() {
    if (MH_Initialize() != MH_OK) {
        Throw("Error initialize Hook's");
    }
    auto wglSwapBuffer = CommonFindSymbolEx("OpenGL32.dll", "wglSwapBuffers");
    printf("wglSwapBuffer %p\n", wglSwapBuffer);

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

    printf("InitializeOverlay\n");
}

void DestroyOverlay() {
    if (g_flag_initialize) {
        if (g_hwnd && g_original_wndproc)
        {
            SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
            g_original_wndproc = nullptr;
            g_hwnd = nullptr;
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_flag_initialize = false;
    }
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

static void UpdateOverlay()
{
    HGLRC context = wglGetCurrentContext();
    if (!context)
        return;
    HDC hdc = wglGetCurrentDC();
    if (!hdc)
        return;
    HWND hwnd = WindowFromDC(hdc);
    if (!hwnd)
        return;
    RECT rect;
    if (!GetClientRect(hwnd, &rect))
        return;
    float width = static_cast<float>(rect.right - rect.left);
    float height = static_cast<float>(rect.bottom - rect.top);
    if (width <= 0.0f || height <= 0.0f)
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("KillMenu");
    ImGui::Text("Hello, World");
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    printf("::UpdateOverlay\n");
}

static bool CreateMenuForWindow(HWND hwnd)
{
  	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplOpenGL3_Init();

	g_hwnd = hwnd;
	g_original_wndproc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Overlay_WndProc)));

    return true;
}

static LRESULT CALLBACK Overlay_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
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

static BOOL WINAPI detour_wglSwapBuffers(HDC unnamedParam1)
{
    if (!g_flag_initialize)
    {
        HWND hwnd = WindowFromDC(unnamedParam1);
        if (hwnd)
        {
            g_flag_initialize = true;
            CreateMenuForWindow(hwnd);
        }
    }


    UpdateOverlay();

    return original_wglSwapBuffers(unnamedParam1);
}