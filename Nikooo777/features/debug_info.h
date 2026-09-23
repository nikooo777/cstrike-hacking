#pragma once

class CUserCmd;

namespace features {

struct ShotAngleTrace;

// One-shot F1 dump. With a command it also prints the weapon, seed, and angle
// state for that tick, plus the shot pipeline when its trace is supplied.
void PrintDebugInfo(const CUserCmd *userCmd = nullptr,
                    const ShotAngleTrace *shotTrace = nullptr);

} // namespace features
