#include "hooks/hooks.h"

#include "core/arch.h"
#include "features/fire_capture.h"
#include "math/vector.h"

namespace hooks {

#if ARCH_X64()
void hkClientFireBullets(int playerIndex, const Vector3 *origin,
                         const Vector3 *fireAngles, int weaponId, int mode,
                         int seed, float inaccuracy, float spread,
                         float soundTime) {
    features::RecordClientFireBullets(
        playerIndex, origin, fireAngles, weaponId, mode, seed, inaccuracy,
        spread, soundTime);
    originalClientFireBullets(playerIndex, origin, fireAngles, weaponId, mode,
                              seed, inaccuracy, spread, soundTime);
}
#endif

} // namespace hooks
