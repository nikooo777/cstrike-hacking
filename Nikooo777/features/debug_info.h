#pragma once

class CUserCmd;

namespace features {

// One-shot F1 dump. When cmd is non-null, also prints seed/cmd-vs-engine angles.
void PrintDebugInfo(const CUserCmd *userCmd = nullptr);

} // namespace features
