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
	float animT;
	bool loading;
	bool loaded;
	std::string statusMsg;
};

static std::vector<ProcessEntry> g_processes;

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
	auto* map = reinterpret_cast<std::map<DWORD, std::string>*>(lParam);
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (!IsWindowVisible(hwnd))
		return TRUE;
	int len = GetWindowTextLengthA(hwnd);
	if (len == 0)
		return TRUE;
	if (map->find(pid) != map->end())
		return TRUE;
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
	if (snapshot == INVALID_HANDLE_VALUE)
		return;

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
				pe.title = (it != titles.end()) ? it->second : "(no window title)";
				pe.animT = 0.0f;
				pe.loading = false;
				pe.loaded = false;
				pe.statusMsg.clear();

				for (auto& old : g_processes) {
					if (old.pid == pe.pid) {
						pe.animT = old.animT;
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
}

static void DrawCard(ProcessEntry& proc) {
	float t = std::min(1.0f, proc.animT);
	float eased = 1.0f - powf(1.0f - t, 3.0f);

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 64.0f);

	ImDrawList* draw = ImGui::GetWindowDrawList();

	ImVec2 offsetPos = ImVec2(pos.x, pos.y + (1.0f - eased) * 12.0f);
	float alpha = eased;

	ImU32 bgColor = ImGui::GetColorU32(ImVec4(0.13f, 0.13f, 0.16f, 0.9f * alpha));
	ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.35f, 0.55f, 0.95f, 0.4f * alpha));

	draw->AddRectFilled(offsetPos, ImVec2(offsetPos.x + size.x, offsetPos.y + size.y), bgColor, 8.0f);
	draw->AddRect(offsetPos, ImVec2(offsetPos.x + size.x, offsetPos.y + size.y), borderColor, 8.0f, 0, 1.5f);

	ImVec2 textPos = ImVec2(offsetPos.x + 16.0f, offsetPos.y + 10.0f);
	draw->AddText(textPos, ImGui::GetColorU32(ImVec4(1, 1, 1, alpha)), proc.name.c_str());

	std::string pidText = "PID: " + std::to_string(proc.pid);
	ImVec2 pidSize = ImGui::CalcTextSize(pidText.c_str());
	ImVec2 pidPos = ImVec2(offsetPos.x + size.x - pidSize.x - 100.0f, offsetPos.y + 10.0f);
	draw->AddText(pidPos, ImGui::GetColorU32(ImVec4(0.6f, 0.8f, 1.0f, alpha)), pidText.c_str());

	ImVec2 titlePos = ImVec2(offsetPos.x + 16.0f, offsetPos.y + 34.0f);
	std::string statusText = proc.title;
	if (!proc.statusMsg.empty())
		statusText += "  |  " + proc.statusMsg;
	draw->AddText(titlePos, ImGui::GetColorU32(ImVec4(0.75f, 0.75f, 0.78f, alpha)), statusText.c_str());

	ImGui::Dummy(ImVec2(size.x, size.y));

	ImGui::SetCursorScreenPos(ImVec2(offsetPos.x + size.x - 80.0f, offsetPos.y + 18.0f));
	ImGui::PushID(static_cast<int>(proc.pid));

	if (proc.loaded) {
		ImGui::BeginDisabled();
		ImGui::Button("Loaded", ImVec2(64, 28));
		ImGui::EndDisabled();
	}
	else if (proc.loading) {
		ImGui::BeginDisabled();
		ImGui::Button("...", ImVec2(64, 28));
		ImGui::EndDisabled();
	}
	else {
		if (ImGui::Button("Load", ImVec2(64, 28))) {
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
	}

	ImGui::PopID();
	ImGui::Dummy(ImVec2(size.x, 8.0f));
}

static void DrawGui(HWND hwnd) {
	RECT rect;
	GetClientRect(hwnd, &rect);

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(
		static_cast<float>(rect.right - rect.left),
		static_cast<float>(rect.bottom - rect.top)));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::Begin("Java Processes", nullptr, flags);

	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%zu process(es) found", g_processes.size());
	ImGui::Separator();
	ImGui::Spacing();

	for (auto& proc : g_processes) {
		if (proc.animT < 1.0f)
			proc.animT += ImGui::GetIO().DeltaTime * 4.0f;
		DrawCard(proc);
	}

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
		L"JavaProcessMonitor", nullptr };
	RegisterClassExW(&wc);

	HWND hwnd = CreateWindowW(wc.lpszClassName, L"openjvmlauncher",
		WS_OVERLAPPEDWINDOW, 100, 100, 560, 520,
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

	ULONGLONG lastRefresh = GetTickCount64();
	RefreshProcessList();

	bool running = true;
	while (running) {
		MSG msg;
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				running = false;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		if (!running)
			break;

		ULONGLONG now = GetTickCount64();
		if (now - lastRefresh > 1500) {
			RefreshProcessList();
			lastRefresh = now;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		CreateBaseImGuiStyle(ImGui::GetIO());
		DrawGui(hwnd);

		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
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