#include "features/menu.h"

#include "features/config.h"
#include "imgui.h"

namespace features {

void Menu() {
    auto &cfg = GetConfig();

    ImGui::Begin("Nikooo777", &cfg.menuOpen);
    ImGui::Checkbox("Bhop", &cfg.bhop);
    ImGui::Checkbox("Aimbot", &cfg.aimbot);
    ImGui::Checkbox("Triggerbot", &cfg.triggerbot);
    ImGui::Checkbox("No recoil (cmd)", &cfg.norecoil);
    ImGui::Checkbox("Visual no recoil (camera)", &cfg.visualNoRecoil);
    ImGui::Checkbox("Perfect nospread (cmd, default off)", &cfg.perfectNoSpread);
    ImGui::Checkbox("Silent angles (restore camera)", &cfg.silentAngles);
    ImGui::Separator();
    ImGui::Text("INSERT: toggle menu");
    ImGui::Text("F1: console debug dump");
    ImGui::Text("END: unload DLL");
    ImGui::End();
}

} // namespace features
