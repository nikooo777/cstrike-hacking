#include "hooks/hooks.h"

#include "core/arch.h"
#include "features/config.h"
#include "features/norecoil.h"
#include "game/entity_list.h"
#include "game/render_state.h"
#include "sdk/view_setup.h"

namespace hooks {

#if ARCH_X86()
void __fastcall hkOverrideView(void *thisPtr, void * /*edx*/,
                               CViewSetup *viewSetup) {
#else
void hkOverrideView(void *thisPtr, CViewSetup *viewSetup) {
#endif
    originalOverrideView(thisPtr, viewSetup);

    if (viewSetup == nullptr) {
        return;
    }

    if (features::GetConfig().visualNoRecoil) {
        features::RecoilState recoil{};
        if (features::ReadRecoilState(game::GetLocalPlayer(), recoil)) {
            viewSetup->angles = viewSetup->angles - recoil.punchAngles;
        }
    }

    game::CaptureViewSetup(*viewSetup);
}

} // namespace hooks
