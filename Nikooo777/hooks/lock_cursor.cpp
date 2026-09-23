#include "hooks/hooks.h"

#include "core/arch.h"
#include "features/config.h"
#include "sdk/vgui_surface.h"

namespace hooks {

#if ARCH_X86()
void __fastcall hkLockCursor(void *thisPtr, void * /*edx*/) {
#else
void hkLockCursor(void *thisPtr) {
#endif
    if (!features::GetConfig().menuOpen) {
        originalLockCursor(thisPtr);
        return;
    }

    sdk::vgui::CallSurfaceMethod(thisPtr, sdk::vgui::kSurfaceUnlockCursor);
    sdk::vgui::CallSurfaceMethod(
        thisPtr, sdk::vgui::kSurfaceSetCursor,
        sdk::vgui::kCursorArrow);
}

} // namespace hooks
