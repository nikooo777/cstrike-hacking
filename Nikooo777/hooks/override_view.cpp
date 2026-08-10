#include "hooks/hooks.h"

#include "features/config.h"
#include "features/norecoil.h"
#include "game/entity_list.h"
#include "sdk/view_setup.h"

namespace hooks {

#if defined(_M_IX86) || defined(__i386__)
void __fastcall hkOverrideView(void *thisPtr, void * /*edx*/,
                               CViewSetup *viewSetup) {
#else
void hkOverrideView(void *thisPtr, CViewSetup *viewSetup) {
#endif
    originalOverrideView(thisPtr, viewSetup);

    if (!features::GetConfig().visualNoRecoil || viewSetup == nullptr) {
        return;
    }

    features::RecoilState recoil{};
    if (!features::ReadRecoilState(game::GetLocalPlayer(), recoil)) {
        return;
    }

    viewSetup->angles = viewSetup->angles - recoil.punchAngles;
}

} // namespace hooks
