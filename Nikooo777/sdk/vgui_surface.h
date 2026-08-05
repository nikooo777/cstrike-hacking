#pragma once

#include <cstddef>

namespace sdk::vgui {

// These are the slots from the target CS:S vguimatsurface.dll vtable. This
// build includes SetCursorAlwaysVisible, so the cursor methods use the newer
// layout despite exposing VGUI_Surface030.
constexpr std::size_t kSurfaceSetCursor = 51;
constexpr std::size_t kSurfaceSetCursorAlwaysVisible = 52;
constexpr std::size_t kSurfaceUnlockCursor = 61;
constexpr std::size_t kSurfaceLockCursor = 62;

// vgui::CursorCode values from public/vgui/Cursor.h.
constexpr unsigned long kCursorNone = 1;
constexpr unsigned long kCursorArrow = 2;

// Ghidra confirms the CS:S VGUI_Input005 vtable address point: slot 0 is the
// scalar-deleting destructor, followed by SetMouseFocus and SetMouseCapture.
constexpr std::size_t kInputSetMouseCapture = 2;

template <typename Return, typename... Args>
Return CallVFunc(void *instance, std::size_t index, Args... args) {
    using Function = Return(__thiscall *)(void *, Args...);
    auto **vtable = *reinterpret_cast<void ***>(instance);
    return reinterpret_cast<Function>(vtable[index])(instance, args...);
}

} // namespace sdk::vgui
