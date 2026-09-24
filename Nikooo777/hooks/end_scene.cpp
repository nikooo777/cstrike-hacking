#include "hooks/hooks.h"

#include <d3d9.h>
#include <iostream>

#include "core/arch.h"
#include "features/config.h"
#include "features/bone_esp.h"
#include "features/menu.h"
#include "features/telemetry.h"
#include "hooks/d3d9_device.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "game/interfaces.h"
#include "sdk/vgui_surface.h"
#include "ui/theme.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace hooks {

namespace {

bool g_imguiInit = false;
bool g_insertWasDown = false;
bool g_menuInputMode = false;
WNDPROC g_originalWndProc = nullptr;
HWND g_gameHwnd = nullptr;

bool IsMenuInputMessage(UINT msg) {
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_INPUT:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_UNICHAR:
        return true;
    default:
        return false;
    }
}

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (features::GetConfig().menuOpen) {
        const LRESULT imguiResult =
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        if (msg == WM_SETCURSOR) {
            SetCursor(LoadCursorA(nullptr, IDC_ARROW));
            return TRUE;
        }
        if (msg == WM_MOUSEACTIVATE) {
            return MA_ACTIVATE;
        }
        if (IsMenuInputMessage(msg)) {
            return 0;
        }
        if (imguiResult != 0) {
            return imguiResult;
        }
    }
    return g_originalWndProc != nullptr
               ? CallWindowProcA(g_originalWndProc, hWnd, msg, wParam, lParam)
               : DefWindowProcA(hWnd, msg, wParam, lParam);
}

void ReleaseMenuMouse() {
    if (auto *surface = game::GetVguiSurface(); surface != nullptr) {
        sdk::vgui::CallSurfaceMethod(
            surface, sdk::vgui::kSurfaceUnlockCursor);
        sdk::vgui::CallSurfaceMethod(
            surface, sdk::vgui::kSurfaceSetCursor,
            sdk::vgui::kCursorArrow);
    }

    if (auto *baseClient = game::GetBaseClient(); baseClient != nullptr) {
        baseClient->IN_DeactivateMouse();
    }
}

void SetMenuInputMode(bool enabled) {
    if (enabled == g_menuInputMode) {
        return;
    }

    g_menuInputMode = enabled;
    if (enabled) {
        ReleaseMenuMouse();
        ClipCursor(nullptr);
        SetCursor(LoadCursorA(nullptr, IDC_ARROW));
    }
}

HWND GetDeviceWindow(IDirect3DDevice9 *device) {
    if (device != nullptr) {
        D3DDEVICE_CREATION_PARAMETERS parameters{};
        if (SUCCEEDED(device->GetCreationParameters(&parameters)) &&
            parameters.hFocusWindow != nullptr) {
            return parameters.hFocusWindow;
        }
    }
    return GetProcessWindow();
}

void PollMenuToggle() {
    const bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insertDown && !g_insertWasDown) {
        auto &menuOpen = features::GetConfig().menuOpen;
        menuOpen = !menuOpen;
        std::cout << "Menu " << (menuOpen ? "opened" : "closed")
                  << " via INSERT" << std::endl;
    }
    g_insertWasDown = insertDown;
}

void InitImGui(IDirect3DDevice9 *device) {
    if (g_imguiInit) {
        return;
    }

    g_gameHwnd = GetDeviceWindow(device);
    if (!g_gameHwnd || !device) {
        return;
    }

    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui::StyleColorsDark();
    ui::theme::ApplyStyle(ImGui::GetStyle());
    ui::theme::LoadFonts(io);
    const bool win32Initialized = ImGui_ImplWin32_Init(g_gameHwnd);
    const bool dx9Initialized = ImGui_ImplDX9_Init(device);
    if (!win32Initialized || !dx9Initialized) {
        std::cout << "ImGui initialization failed (Win32="
                  << (win32Initialized ? "ok" : "failed")
                  << ", DX9=" << (dx9Initialized ? "ok" : "failed")
                  << ")" << std::endl;
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return;
    }

    SetLastError(ERROR_SUCCESS);
    g_originalWndProc =
        (WNDPROC)SetWindowLongPtrA(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    const DWORD windowProcError = GetLastError();
    if (g_originalWndProc == nullptr && windowProcError != ERROR_SUCCESS) {
        std::cout << "ImGui window procedure hook failed ("
                  << windowProcError << "); keyboard polling remains active"
                  << std::endl;
    }
    g_imguiInit = true;
    std::cout << "ImGui initialized on HWND 0x" << std::hex << g_gameHwnd
              << std::dec << std::endl;
}

void ShutdownImGui() {
    if (!g_imguiInit) {
        return;
    }

    if (g_gameHwnd && g_originalWndProc) {
        SetWindowLongPtrA(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
    }
    g_originalWndProc = nullptr;
    g_gameHwnd = nullptr;
    g_insertWasDown = false;
    SetMenuInputMode(false);

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_imguiInit = false;
}

void ReleaseDeviceObjects() {
    if (g_imguiInit) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
}

} // namespace

// ImGui's DX9 backend keeps D3DPOOL_DEFAULT buffers and its font texture;
// Reset fails while such resources exist. Release them first; NewFrame
// recreates them on the next frame after a successful reset.
HRESULT __stdcall hkReset(IDirect3DDevice9 *device,
                          D3DPRESENT_PARAMETERS *parameters) {
    ReleaseDeviceObjects();
    return originalReset(device, parameters);
}

HRESULT __stdcall hkEndScene(IDirect3DDevice9 *device) {
    features::telemetry::CountCall(features::telemetry::Hook::EndScene);
    PollMenuToggle();
    SetMenuInputMode(features::GetConfig().menuOpen);
    InitImGui(device);

    if (g_imguiInit) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (features::GetConfig().menuOpen) {
            ImGui::GetIO().MouseDrawCursor = false;
            features::Menu();
        }

        D3DVIEWPORT9 viewport{};
        if (device == nullptr || FAILED(device->GetViewport(&viewport))) {
            viewport = {};
        }
        features::DrawBoneEsp(static_cast<float>(viewport.X),
                              static_cast<float>(viewport.Y),
                              static_cast<float>(viewport.Width),
                              static_cast<float>(viewport.Height));

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
