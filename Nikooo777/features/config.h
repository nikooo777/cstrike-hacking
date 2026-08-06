#pragma once

#include "config/config.h"

namespace features {

// Runtime toggles (ImGui menu + defaults for offline testing).
struct Config {
    bool bhop = true;
    bool aimbot = true;
    bool triggerbot = true;
    bool norecoil = true;
    bool visualNoRecoil = true;
    bool perfectNoSpread = false;
    bool silentAngles = true;
    bool menuOpen = false; // INSERT toggles; closed by default so inject is quiet
};

inline Config &GetConfig() {
    static Config cfg;
    return cfg;
}

inline void ApplyConfig(const config::Config &source) {
    auto &target = GetConfig();
    target.bhop = source.features.bhop;
    target.aimbot = source.features.aimbot;
    target.triggerbot = source.features.triggerbot;
    target.norecoil = source.features.norecoil;
    target.visualNoRecoil = source.features.visualNoRecoil;
    target.perfectNoSpread = source.features.perfectNoSpread;
    target.silentAngles = source.features.silentAngles;
    target.menuOpen = source.features.menuOpen;
}

} // namespace features
