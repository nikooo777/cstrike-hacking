#include "ui/theme.h"

#include <algorithm>

#include "ui/fonts/chakra_petch_medium.h"
#include "ui/fonts/chakra_petch_semibold.h"

namespace ui::theme {

namespace {

Fonts g_fonts{};

ImVec4 ToVec4(ImU32 color) {
    return ImGui::ColorConvertU32ToFloat4(color);
}

} // namespace

Fonts LoadFonts(ImGuiIO &io) {
    ImFontConfig config{};
    config.OversampleH = 3;
    config.OversampleV = 1;
    config.PixelSnapH = true;

    g_fonts.label = io.Fonts->AddFontFromMemoryCompressedBase85TTF(
        chakra_petch_medium_compressed_data_base85, 16.0f, &config);
    g_fonts.small = io.Fonts->AddFontFromMemoryCompressedBase85TTF(
        chakra_petch_medium_compressed_data_base85, 14.0f, &config);
    g_fonts.category = io.Fonts->AddFontFromMemoryCompressedBase85TTF(
        chakra_petch_semibold_compressed_data_base85, 18.0f, &config);
    g_fonts.title = io.Fonts->AddFontFromMemoryCompressedBase85TTF(
        chakra_petch_semibold_compressed_data_base85, 24.0f, &config);
    return g_fonts;
}

const Fonts &GetFonts() {
    return g_fonts;
}

void ApplyStyle(ImGuiStyle &style) {
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupRounding = 0.0f;
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.Colors[ImGuiCol_Text] = ToVec4(kText);
    style.Colors[ImGuiCol_TextDisabled] = ToVec4(kTextDim);
    style.Colors[ImGuiCol_WindowBg] = ToVec4(kGlass);
    style.Colors[ImGuiCol_PopupBg] = ToVec4(kGlassRaised);
    style.Colors[ImGuiCol_Border] = ToVec4(kSteelFaint);
}

ImU32 WithAlpha(ImU32 color, float factor) {
    const auto alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF);
    const auto scaled = static_cast<ImU32>(
        std::clamp(alpha * factor, 0.0f, 255.0f) + 0.5f);
    return (color & ~IM_COL32_A_MASK) | (scaled << IM_COL32_A_SHIFT);
}

} // namespace ui::theme
