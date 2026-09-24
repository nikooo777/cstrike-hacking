#pragma once

#include "imgui.h"
#include "ui/wheel_geometry.h"

namespace ui::theme {

// Smoked glass keeps the wheel legible over bright and dark maps; amber is
// the CS:S HUD numeral color and marks everything that is on or selected.
inline constexpr ImU32 kGlass = IM_COL32(20, 26, 36, 178);
inline constexpr ImU32 kGlassRaised = IM_COL32(38, 49, 66, 214);
inline constexpr ImU32 kSteel = IM_COL32(143, 163, 191, 255);
inline constexpr ImU32 kSteelFaint = IM_COL32(143, 163, 191, 72);
inline constexpr ImU32 kAmber = IM_COL32(255, 181, 71, 255);
inline constexpr ImU32 kAmberWash = IM_COL32(255, 181, 71, 40);
inline constexpr ImU32 kAmberFill = IM_COL32(255, 181, 71, 64);
inline constexpr ImU32 kFault = IM_COL32(255, 93, 93, 255);
inline constexpr ImU32 kText = IM_COL32(230, 238, 248, 255);
inline constexpr ImU32 kVignette = IM_COL32(6, 9, 14, 110);
inline constexpr ImU32 kTextDim = IM_COL32(154, 167, 184, 255);

inline constexpr float kDegree = geometry::kPi / 180.0f;

// Designed at 1080p, in pixels from the wheel center.
inline constexpr float kCenterRadius = 118.0f;
inline constexpr float kItemInner = 126.0f;
inline constexpr float kItemOuter = 176.0f;
inline constexpr float kCategoryInner = 184.0f;
inline constexpr float kCategoryOuter = 216.0f;
inline constexpr float kChamfer = 6.0f;
inline constexpr float kTickOffset = 4.0f;
inline constexpr float kTickLength = 6.0f;
inline constexpr float kTickSpacing = 2.6f * kDegree;
inline constexpr float kCaretOffset = 17.0f;
inline constexpr float kCaretLength = 8.0f;
inline constexpr float kCaretWidth = 11.0f;
inline constexpr float kVignetteRadius = 330.0f;
inline constexpr float kCaptionOffset = 36.0f;
inline constexpr float kCategoryGap = 2.4f * kDegree;
inline constexpr float kItemGap = 1.4f * kDegree;
inline constexpr float kItemSpan = 36.0f * kDegree;
inline constexpr float kItemMaxSpan = 150.0f * kDegree;
inline constexpr float kHairline = 1.0f;
inline constexpr float kEdge = 2.0f;

inline constexpr double kOpenSeconds = 0.16;
inline constexpr double kFanSeconds = 0.12;

struct Fonts {
    ImFont *small = nullptr;
    ImFont *label = nullptr;
    ImFont *category = nullptr;
    ImFont *title = nullptr;
};

// Adds the embedded Chakra Petch faces to the atlas. Call before the first
// frame; the first font becomes ImGui's default.
Fonts LoadFonts(ImGuiIO &io);
const Fonts &GetFonts();

void ApplyStyle(ImGuiStyle &style);

ImU32 WithAlpha(ImU32 color, float factor);

} // namespace ui::theme
