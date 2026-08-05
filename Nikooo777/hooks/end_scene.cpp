#include "hooks/hooks.h"

#include <d3d9.h>

#include "features/config.h"
#include "features/menu.h"
#include "hooks/d3d9_device.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace hooks {

namespace {

bool g_imguiInit = false;
WNDPROC g_originalWndProc = nullptr;

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (features::GetConfig().menuOpen && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
    return CallWindowProcA(g_originalWndProc, hWnd, msg, wParam, lParam);
}

void InitImGui(IDirect3DDevice9 *device) {
    if (g_imguiInit) {
        return;
    }

    HWND hwnd = GetProcessWindow();
    if (!hwnd || !device) {
        return;
    }

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(device);

    g_originalWndProc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    g_imguiInit = true;
}

void ShutdownImGui() {
    if (!g_imguiInit) {
        return;
    }

    HWND hwnd = GetProcessWindow();
    if (hwnd && g_originalWndProc) {
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
        g_originalWndProc = nullptr;
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_imguiInit = false;
}

} // namespace

HRESULT __stdcall hkEndScene(IDirect3DDevice9 *device) {
    InitImGui(device);

    if (g_imguiInit) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        features::Menu();

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    return originalEndScene(device);
}

void ShutdownEndScene() {
    ShutdownImGui();
}

} // namespace hooks
