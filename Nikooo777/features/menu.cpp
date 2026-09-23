#include "features/menu.h"

#include <atomic>

#include "features/config.h"
#include "imgui.h"

namespace features {

namespace {

void Toggle(const char *label, std::atomic<bool> &value) {
    bool current = value;
    if (ImGui::Checkbox(label, &current)) {
        value = current;
    }
}

} // namespace

void Menu() {
    auto &cfg = GetConfig();

    bool open = cfg.menuOpen;
    ImGui::Begin("Nikooo777", &open);
    Toggle("Bhop", cfg.bhop);
    Toggle("Aimbot", cfg.aimbot);
    Toggle("Triggerbot", cfg.triggerbot);
    Toggle("No recoil (cmd)", cfg.norecoil);
    Toggle("Visual no recoil (camera)", cfg.visualNoRecoil);
    Toggle("Perfect nospread (cmd, default off)", cfg.perfectNoSpread);
    Toggle("Silent angles (restore camera)", cfg.silentAngles);
    Toggle("Bone ESP (experimental)", cfg.boneEsp);
    ImGui::Separator();
    ImGui::Text("INSERT: toggle menu");
    ImGui::Text("F1: console debug dump");
    ImGui::Text("END: unload DLL");
    ImGui::End();
    if (!open) {
        cfg.menuOpen = false;
    }
}

} // namespace features
