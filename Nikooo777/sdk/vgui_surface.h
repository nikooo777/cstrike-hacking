#pragma once

#include <cstddef>

namespace sdk::vgui {

// Verified against VGUI_Surface030 in the target x86 and x64 binaries.
constexpr std::size_t kSurfaceSetCursor = 51;
constexpr std::size_t kSurfaceUnlockCursor = 61;
constexpr std::size_t kSurfaceLockCursor = 62;

// vgui::CursorCode values from public/vgui/Cursor.h.
constexpr unsigned long kCursorArrow = 2;

template <typename Return, typename... Args>
Return CallVFunc(void *instance, std::size_t index, Args... args) {
    using Function = Return(__thiscall *)(void *, Args...);
    auto **vtable = *reinterpret_cast<void ***>(instance);
    return reinterpret_cast<Function>(vtable[index])(instance, args...);
}

} // namespace sdk::vgui
