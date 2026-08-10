#pragma once

class CUserCmd;
class Vector3;

namespace features {

// One-shot F1 dump. When cmd is non-null, also prints seed/cmd-vs-engine angles.
void PrintDebugInfo(const CUserCmd *userCmd = nullptr);
void ArmClientFireDiagnostic();
void CaptureClientFireCommand(const CUserCmd *userCmd);
void RecordClientFireBullets(int playerIndex, const Vector3 *origin,
                             const Vector3 *fireAngles, int weaponId,
                             int mode, int seed, float inaccuracy,
                             float spread, float soundTime);
void RecordAccuracyPenaltyUpdate(void *weapon, bool beforeCall);

} // namespace features
