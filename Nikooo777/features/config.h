#pragma once

#include <atomic>

#include "config/config.h"

namespace features {

// Runtime toggles. The menu and INSERT handling write them from EndScene and
// the window procedure, while CreateMove and OverrideView read them, so each
// one is atomic.
struct Config {
    std::atomic<bool> bhop{true};
    std::atomic<bool> aimbot{true};
    std::atomic<bool> triggerbot{true};
    std::atomic<bool> norecoil{true};
    std::atomic<bool> visualNoRecoil{true};
    std::atomic<bool> perfectNoSpread{false};
    std::atomic<bool> silentAngles{true};
    std::atomic<bool> boneEsp{false};
    std::atomic<bool> menuOpen{false}; // INSERT toggles; closed by default so inject is quiet
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
    target.boneEsp = source.features.boneEsp;
    target.menuOpen = source.features.menuOpen;
}

} // namespace features
