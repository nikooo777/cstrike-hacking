#include "features/menu.h"

#include "features/config.h"
#include "imgui.h"

namespace features {

void Menu() {
    auto &cfg = GetConfig();
    if (!cfg.menuOpen) {
        return;
    }

    ImGui::Begin("Nikooo777", &cfg.menuOpen);
    ImGui::Checkbox("Bhop", &cfg.bhop);
    ImGui::Checkbox("Aimbot", &cfg.aimbot);
    ImGui::Checkbox("Triggerbot", &cfg.triggerbot);
    ImGui::Checkbox("No recoil (cmd)", &cfg.norecoil);
    ImGui::Checkbox("Visual no recoil (FSN)", &cfg.visualNoRecoil);
    ImGui::Text("INSERT: toggle menu | END: unload");
    ImGui::End();
}

} // namespace features
