#pragma once

#include <Windows.h>
#include <d3d9.h>

namespace hooks {

HWND GetProcessWindow();
bool GetD3D9Device(void **pTable, size_t size);
void CleanupDummyD3D();

} // namespace hooks
