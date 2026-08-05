#pragma once

namespace features {

// Simple runtime toggles (driven by the ImGui menu).
struct Config {
    bool bhop = true;
    bool aimbot = true;
    bool triggerbot = true;
    bool norecoil = true;
    bool visualNoRecoil = true;
    bool menuOpen = true;
};

inline Config &GetConfig() {
    static Config cfg;
    return cfg;
}

} // namespace features
