#pragma once

#include <cstddef>

#include "core/arch.h"
#include "memory/mem.h"

namespace sdk::vgui {

// Verified against VGUI_Surface030 in the target x86 and x64 binaries.
constexpr std::size_t kSurfaceSetCursor = 51;
constexpr std::size_t kSurfaceUnlockCursor = 61;
constexpr std::size_t kSurfaceLockCursor = 62;

// vgui::CursorCode values from public/vgui/Cursor.h.
constexpr unsigned long kCursorArrow = 2;

template <typename... Args>
bool CallSurfaceMethod(void *surface, std::size_t slot, Args... args) {
    using Function = void(ARCH_THISCALL *)(void *, Args...);
    const auto function = mem::GetVirtual<Function>(surface, slot);
    if (function == nullptr) {
        return false;
    }
    __try {
        function(surface, args...);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

} // namespace sdk::vgui
