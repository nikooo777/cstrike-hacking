#include "hooks/hooks.h"

#include <d3d9.h>
#include <iostream>

#include "features/config.h"
#include "features/menu.h"
#include "hooks/d3d9_device.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "game/interfaces.h"
#include "sdk/create_interface.h"
#include "sdk/vgui_surface.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace hooks {

namespace {

bool g_imguiInit = false;
bool g_insertWasDown = false;
bool g_menuInputMode = false;
bool g_vguiLookupAttempted = false;
WNDPROC g_originalWndProc = nullptr;
HWND g_gameHwnd = nullptr;
void *g_vguiSurface = nullptr;
void *g_vguiInput = nullptr;

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

void ApplyVguiInputMode(bool enabled, bool updateCursorVisibilityOverride) {
    if (!g_vguiLookupAttempted) {
        g_vguiLookupAttempted = true;
        g_vguiSurface = GetInterface("vguimatsurface.dll", "VGUI_Surface030");
        g_vguiInput = GetInterface("vgui2.dll", "VGUI_Input005");
    }

    if (g_vguiSurface != nullptr) {
        if (enabled) {
            // Source's CalculateMouseVisible path unlocks before changing the
            // cursor state; preserve that ordering here.
            sdk::vgui::CallVFunc<void>(
                g_vguiSurface, sdk::vgui::kSurfaceUnlockCursor);
            if (updateCursorVisibilityOverride) {
                // This target keeps a visibility reference count, so only
                // change the override when the menu state changes.
                sdk::vgui::CallVFunc<void>(
                    g_vguiSurface,
                    sdk::vgui::kSurfaceSetCursorAlwaysVisible,
                    true);
            }
            sdk::vgui::CallVFunc<void>(
                g_vguiSurface, sdk::vgui::kSurfaceSetCursor,
                sdk::vgui::kCursorArrow);
        } else {
            if (updateCursorVisibilityOverride) {
                sdk::vgui::CallVFunc<void>(
                    g_vguiSurface,
                    sdk::vgui::kSurfaceSetCursorAlwaysVisible,
                    false);
            }
            sdk::vgui::CallVFunc<void>(
                g_vguiSurface, sdk::vgui::kSurfaceSetCursor,
                sdk::vgui::kCursorNone);
            sdk::vgui::CallVFunc<void>(
                g_vguiSurface, sdk::vgui::kSurfaceLockCursor);
        }
    } else {
        std::cout << "VGUI surface interface unavailable; using Win32 cursor fallback"
                  << std::endl;
    }

    if (auto *baseClient = game::GetBaseClient(); baseClient != nullptr) {
        if (enabled) {
            // This is the engine-supported transition that stops CInput from
            // recentering the OS cursor while a UI is active. Keep applying it
            // while open because the game may reactivate input during a frame.
            baseClient->IN_DeactivateMouse();
        } else {
            baseClient->IN_ActivateMouse();
        }
    }

    if (g_vguiInput != nullptr) {
        sdk::vgui::CallVFunc<void>(
            g_vguiInput, sdk::vgui::kInputSetMouseCapture,
            static_cast<void *>(nullptr));
    }
}

void SetWin32CursorVisible(bool visible) {
    int result = ShowCursor(visible ? TRUE : FALSE);
    for (int i = 1; i < 16; ++i) {
        if ((visible && result >= 0) || (!visible && result < 0)) {
            break;
        }
        result = ShowCursor(visible ? TRUE : FALSE);
    }
}

void SetMenuInputMode(bool enabled) {
    if (enabled == g_menuInputMode) {
        if (enabled) {
            ApplyVguiInputMode(true, false);
            ClipCursor(nullptr);
        }
        return;
    }

    g_menuInputMode = enabled;
    ApplyVguiInputMode(enabled, true);

    if (enabled) {
        // Let the OS cursor move freely instead of following the game's
        // first-person/raw-input capture while the menu is open.
        ClipCursor(nullptr);
        SetCursor(LoadCursorA(nullptr, IDC_ARROW));
        SetWin32CursorVisible(true);
    } else {
        ClipCursor(nullptr);
        SetWin32CursorVisible(false);
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
    ImGui::StyleColorsDark();
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

} // namespace

HRESULT __stdcall hkEndScene(IDirect3DDevice9 *device) {
    PollMenuToggle();
    SetMenuInputMode(features::GetConfig().menuOpen);
    InitImGui(device);

    // Skip the whole frame path when the menu is closed; INSERT is polled
    // outside the ImGui frame path.
    if (g_imguiInit && features::GetConfig().menuOpen) {
        ImGui::GetIO().MouseDrawCursor = false;
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
