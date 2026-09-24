#include "hooks/d3d9_device.h"

#include <cstddef>

namespace hooks {

namespace {

constexpr std::size_t kResetVtableIndex = 16;
constexpr std::size_t kResetExVtableIndex = 132;

HWND g_window = nullptr;
IDirect3D9 *g_pD3D = nullptr;
IDirect3DDevice9 *g_pDevice = nullptr;

// Prefer a visible top-level window for this process (game main window), not the first HWND.
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM) {
    DWORD wndProcId = 0;
    GetWindowThreadProcessId(handle, &wndProcId);
    if (GetCurrentProcessId() != wndProcId) {
        return TRUE;
    }
    if (!IsWindowVisible(handle)) {
        return TRUE;
    }
    // Skip tiny tool windows / owned popups when a real main window exists.
    if (GetWindow(handle, GW_OWNER) != nullptr) {
        return TRUE;
    }
    RECT rc{};
    if (!GetClientRect(handle, &rc) || (rc.right - rc.left) < 100 || (rc.bottom - rc.top) < 100) {
        return TRUE;
    }
    g_window = handle;
    return FALSE;
}

} // namespace

HWND GetProcessWindow() {
    g_window = nullptr;
    EnumWindows(EnumWindowsCallback, NULL);
    return g_window;
}

bool GetD3D9Device(void **pTable, size_t size) {
    if (!pTable) {
        return false;
    }

    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_pD3D) {
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = GetProcessWindow();
    d3dpp.Windowed = TRUE;

    HRESULT res = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow,
                                       D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pDevice);
    if (FAILED(res) || !g_pDevice) {
        CleanupDummyD3D();
        return false;
    }

    memcpy(pTable, *reinterpret_cast<void ***>(g_pDevice), size);
    CleanupDummyD3D();
    return true;
}

void CleanupDummyD3D() {
    if (g_pDevice) {
        g_pDevice->Release();
        g_pDevice = nullptr;
    }
    if (g_pD3D) {
        g_pD3D->Release();
        g_pD3D = nullptr;
    }
}

D3D9ExSlots GetD3D9ExSlots() {
    D3D9ExSlots slots;
    HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
    if (d3d9 == nullptr) {
        return slots;
    }

    using CreateD3D9ExFn = HRESULT(WINAPI *)(UINT, IDirect3D9Ex **);
    const auto create = reinterpret_cast<CreateD3D9ExFn>(
        GetProcAddress(d3d9, "Direct3DCreate9Ex"));
    IDirect3D9Ex *d3d = nullptr;
    if (create == nullptr || FAILED(create(D3D_SDK_VERSION, &d3d)) ||
        d3d == nullptr) {
        return slots;
    }

    D3DPRESENT_PARAMETERS parameters = {};
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = GetProcessWindow();
    parameters.Windowed = TRUE;
    IDirect3DDevice9Ex *device = nullptr;
    if (SUCCEEDED(d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                      parameters.hDeviceWindow,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &parameters, nullptr, &device)) &&
        device != nullptr) {
        void **vtable = *reinterpret_cast<void ***>(device);
        slots.reset = vtable[kResetVtableIndex];
        slots.resetEx = vtable[kResetExVtableIndex];
        device->Release();
    }
    d3d->Release();
    return slots;
}

} // namespace hooks
