#include "hooks/d3d9_device.h"

namespace hooks {

namespace {

HWND g_window = nullptr;
IDirect3D9 *g_pD3D = nullptr;
IDirect3DDevice9 *g_pDevice = nullptr;

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM) {
    DWORD wndProcId = 0;
    GetWindowThreadProcessId(handle, &wndProcId);
    if (GetCurrentProcessId() != wndProcId) {
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

} // namespace hooks
