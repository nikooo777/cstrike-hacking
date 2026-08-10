#include "hooks/hooks.h"

#include "features/debug_info.h"

namespace hooks {

#if !defined(_M_IX86) && !defined(__i386__)
void hkUpdateAccuracyPenalty(void *weapon) {
    features::RecordAccuracyPenaltyUpdate(weapon, true);
    originalUpdateAccuracyPenalty(weapon);
    features::RecordAccuracyPenaltyUpdate(weapon, false);
}
#endif

}
