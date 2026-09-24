#pragma once

#include <Windows.h>
#include <d3d9.h>

namespace hooks {

HWND GetProcessWindow();
bool GetD3D9Device(void **pTable, size_t size);
void CleanupDummyD3D();

struct D3D9ExSlots {
    void *reset = nullptr;
    void *resetEx = nullptr;
};

// Creates a throwaway D3D9Ex device to read its Reset and ResetEx entries.
// Returns empty slots when the system cannot create a D3D9Ex device.
D3D9ExSlots GetD3D9ExSlots();

} // namespace hooks
