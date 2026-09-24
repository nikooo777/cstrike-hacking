#pragma once

#include <vector>

#include "ui/wheel.h"

namespace ui {

enum class MenuItem {
    Aimbot,
    Triggerbot,
    NoRecoil,
    NoSpread,
    SilentAngles,
    VisualNoRecoil,
    BoneEsp,
    Bhop,
    Hooks,
    Interfaces,
    ShotPipeline,
    BoneEspStages,
    PrintDiagnostics,
};

// The wheel's fixed text. The caller fills in each item's live state and any
// dynamic details, looked up through `ids`.
struct MenuContent {
    WheelModel model;
    std::vector<std::vector<MenuItem>> ids;
};

MenuContent BuildMenuContent();

WheelItem *FindItem(MenuContent &content, MenuItem id);

} // namespace ui
