#include "hooks/hooks.h"

#include "core/arch.h"
#include "features/fire_capture.h"

namespace hooks {

#if ARCH_X64()
void hkUpdateAccuracyPenalty(void *weapon) {
    features::RecordAccuracyPenaltyUpdate(weapon, true);
    originalUpdateAccuracyPenalty(weapon);
    features::RecordAccuracyPenaltyUpdate(weapon, false);
}
#endif

}
