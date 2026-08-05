#pragma once

namespace features {

// Runtime toggles (ImGui menu + defaults for offline testing).
struct Config {
    bool bhop = true;
    bool aimbot = true;
    bool triggerbot = true;
    bool norecoil = true;
    bool visualNoRecoil = true;
    bool menuOpen = false; // INSERT toggles; closed by default so inject is quiet
};

inline Config &GetConfig() {
    static Config cfg;
    return cfg;
}

} // namespace features
