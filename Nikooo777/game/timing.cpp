#include "game/timing.h"

#include <cmath>
#include <cstdint>

#include "config/config.h"
#include "game/interfaces.h"
#include "memory/mem.h"

namespace game {

bool GetIntervalPerTick(float &interval) {
    interval = 0.0f;

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    static bool attempted = false;
    static std::uintptr_t globalVarsSlot = 0;
    if (!attempted) {
        attempted = true;
        if (config::IsLoaded()) {
            const auto &signature = config::Get().globalVars;
            std::size_t matchCount = 0;
            auto *match = FindConfiguredSignature(signature, matchCount);
            if (match != nullptr && signature.operand == "rip_rel32" &&
                signature.indirections == 1) {
                mem::DecodeRipRelative32(
                    match, signature.operandOffset,
                    signature.instructionOffset,
                    signature.instructionLength, globalVarsSlot);
            }
        }
    }

    std::uintptr_t globalVars = 0;
    if (globalVarsSlot == 0 ||
        !mem::ReadValue(reinterpret_cast<const void *>(globalVarsSlot),
                        globalVars) ||
        globalVars == 0 ||
        !mem::ReadValue(reinterpret_cast<const void *>(globalVars + 0x1c),
                        interval) ||
        !std::isfinite(interval) || interval <= 0.0f || interval > 1.0f) {
        interval = 0.0f;
        return false;
    }
    return true;
#else
    return false;
#endif
}

}
