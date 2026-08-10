#include "hooks/hooks.h"

#include "features/config.h"
#include "sdk/vgui_surface.h"

namespace hooks {

#if defined(_M_IX86) || defined(__i386__)
void __fastcall hkLockCursor(void *thisPtr, void * /*edx*/) {
#else
void hkLockCursor(void *thisPtr) {
#endif
    if (!features::GetConfig().menuOpen) {
        originalLockCursor(thisPtr);
        return;
    }

    sdk::vgui::CallVFunc<void>(thisPtr, sdk::vgui::kSurfaceUnlockCursor);
    sdk::vgui::CallVFunc<void>(
        thisPtr, sdk::vgui::kSurfaceSetCursor,
        sdk::vgui::kCursorArrow);
}

} // namespace hooks
