#include "ui/menu_content.h"

#include <utility>

namespace ui {

namespace {

WheelItem Toggle(std::string label, std::string title, std::string summary,
                 std::vector<std::string> details, bool experimental = false) {
    WheelItem item;
    item.label = std::move(label);
    item.title = std::move(title);
    item.summary = std::move(summary);
    item.details = std::move(details);
    item.kind = ItemKind::Toggle;
    item.experimental = experimental;
    return item;
}

WheelItem Status(std::string label, std::string title, std::string summary) {
    WheelItem item;
    item.label = std::move(label);
    item.title = std::move(title);
    item.summary = std::move(summary);
    item.kind = ItemKind::Status;
    item.state = ItemState::Idle;
    return item;
}

WheelItem Action(std::string label, std::string title, std::string summary,
                 std::vector<std::string> details) {
    WheelItem item;
    item.label = std::move(label);
    item.title = std::move(title);
    item.summary = std::move(summary);
    item.details = std::move(details);
    item.kind = ItemKind::Action;
    return item;
}

} // namespace

MenuContent BuildMenuContent() {
    MenuContent content;
    auto add = [&content](std::string label, std::string summary,
                          std::vector<std::pair<MenuItem, WheelItem>> items) {
        WheelCategory category;
        category.label = std::move(label);
        category.summary = std::move(summary);
        std::vector<MenuItem> ids;
        for (auto &entry : items) {
            ids.push_back(entry.first);
            category.items.push_back(std::move(entry.second));
        }
        content.model.categories.push_back(std::move(category));
        content.ids.push_back(std::move(ids));
    };

    add("Aim", "Help with finding and hitting targets.",
        {{MenuItem::Aimbot,
          Toggle("Aimbot", "Aimbot",
                 "While you hold mouse 1, aims at the closest visible "
                 "enemy's head.",
                 {"Evidence: aidocs 004 section 6.2"})},
         {MenuItem::Triggerbot,
          Toggle("Trigger", "Triggerbot",
                 "While you hold Shift, fires when your crosshair is on an "
                 "enemy.",
                 {"x64 input path not validated yet",
                  "Evidence: aidocs 004 section 9.3"})}});
    add("Accuracy", "Shape where each shot goes.",
        {{MenuItem::NoRecoil,
          Toggle("No-recoil", "Command no-recoil",
                 "Cancels the recoil kick in the shot you send.",
                 {"Evidence: aidocs 005 section 8"})},
         {MenuItem::NoSpread,
          Toggle("No-spread", "Perfect no-spread",
                 "Aims each shot so the spread cone lands it on target.",
                 {"Validated for one x64 build",
                  "Evidence: aidocs 005 section 8.2"},
                 true)},
         {MenuItem::SilentAngles,
          Toggle("Silent", "Silent angles",
                 "Keeps corrected angles off your camera.",
                 {"Evidence: aidocs 005 section 9"})}});
    add("Visuals", "Change what you see, not what you send.",
        {{MenuItem::VisualNoRecoil,
          Toggle("Camera", "Visual no-recoil",
                 "Removes the recoil kick from your view only.",
                 {"Evidence: aidocs 005 section 9.4"})},
         {MenuItem::BoneEsp,
          Toggle("Bone ESP", "Bone ESP",
                 "Draws enemy skeletons from their cached bones.",
                 {"Evidence: aidocs 006"}, true)}});
    add("Movement", "Timing help for movement.",
        {{MenuItem::Bhop,
          Toggle("Bhop", "Bunny hop",
                 "While you hold Space, jumps again the moment you land.",
                 {"Evidence: aidocs 003"})}});
    add("Status", "See whether each part of the DLL is working.",
        {{MenuItem::Hooks,
          Status("Hooks", "Hooks", "How often each hook ran in the last second.")},
         {MenuItem::Interfaces,
          Status("Setup", "Optional interfaces",
                 "Engine interfaces and diagnostics found at startup.")},
         {MenuItem::ShotPipeline,
          Status("Shots", "Shot pipeline",
                 "The last command's aim, recoil and spread stages.")},
         {MenuItem::BoneEspStages,
          Status("ESP", "Bone ESP stages",
                 "Where the overlay's data comes from, stage by stage.")},
         {MenuItem::PrintDiagnostics,
          Action("Dump", "Print diagnostics",
                 "Writes the full F1 report to the console.",
                 {"Same as pressing F1"})}});

    content.model.hint = "Insert closes the wheel. Scroll to change category.";
    return content;
}

WheelItem *FindItem(MenuContent &content, MenuItem id) {
    for (std::size_t category = 0; category < content.ids.size(); ++category) {
        for (std::size_t item = 0; item < content.ids[category].size(); ++item) {
            if (content.ids[category][item] == id) {
                return &content.model.categories[category].items[item];
            }
        }
    }
    return nullptr;
}

} // namespace ui
