#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <cstddef>

namespace hooks {

inline constexpr std::size_t kResetVtableIndex = 16;
inline constexpr std::size_t kEndSceneVtableIndex = 42;

HWND GetProcessWindow();
bool GetD3D9Device(void **pTable, size_t size);
void CleanupDummyD3D();

struct ExFactorySlots {
    void *reset = nullptr;
    void *endScene = nullptr;
};

// shaderapidx9 creates its device with CreateDevice on a Direct3DCreate9Ex
// factory (aidocs/007 section 7.1). Builds a throwaway device the same way
// and reads its Reset and EndScene entries. Returns empty slots when the
// system has no D3D9Ex factory.
ExFactorySlots GetExFactorySlots();

} // namespace hooks
