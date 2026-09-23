#pragma once

#include "core/arch.h"

class CUserCmd;
class Vector3;

namespace features {

struct ShotAngleTrace;

// One-shot x64 diagnostic armed by F1: capture the next firing command, then
// compare it with the game's own FX_FireBullets and UpdateAccuracyPenalty
// calls (aidocs/005 section 11.2).
#if ARCH_X64()
void ArmClientFireDiagnostic();
void CaptureClientFireCommand(const CUserCmd *userCmd,
                              const ShotAngleTrace &shotTrace);
void RecordClientFireBullets(int playerIndex, const Vector3 *origin,
                             const Vector3 *fireAngles, int weaponId,
                             int mode, int seed, float inaccuracy,
                             float spread, float soundTime);
void RecordAccuracyPenaltyUpdate(void *weapon, bool beforeCall);
#endif

const char *ClientFireCaptureStatus();

} // namespace features
