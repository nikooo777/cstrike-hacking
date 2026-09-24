#pragma once

#include <string>
#include <vector>

#include "imgui.h"
#include "ui/theme.h"

namespace ui {

enum class ItemKind { Toggle, Status, Action };
enum class ItemState { On, Off, Ok, Idle, Fault };

struct WheelItem {
    std::string label;   // short enough for the ring segment
    std::string title;   // full name for the center readout
    std::string summary; // what the item does, in one sentence
    std::vector<std::string> details;
    ItemKind kind = ItemKind::Toggle;
    ItemState state = ItemState::Off;
    bool experimental = false;
};

struct WheelCategory {
    std::string label;
    std::string summary;
    std::vector<WheelItem> items;
};

struct WheelModel {
    std::vector<WheelCategory> categories;
    std::string hint;
};

struct WheelState {
    int selectedCategory = 0;
    double openedAt = 0.0;
    double categoryChangedAt = 0.0;
};

struct WheelInput {
    ImVec2 center{};
    ImVec2 mouse{};
    bool clicked = false;
    float scroll = 0.0f;
    double time = 0.0;
};

struct WheelResult {
    int category = -1;
    int item = -1;
};

// Draws the two-ring wheel and applies category selection to `state`. The
// result names an item the user activated this frame, if any.
WheelResult DrawWheel(ImDrawList *drawList, const theme::Fonts &fonts,
                      const WheelModel &model, WheelState &state,
                      const WheelInput &input);

} // namespace ui
