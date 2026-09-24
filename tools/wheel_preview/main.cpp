// Renders the menu wheel with sample state to PNG files, so the design can be
// reviewed without loading the game (aidocs/007).
//
// Usage: wheel_preview <output directory>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "raster.h"
#include "ui/menu_content.h"
#include "ui/theme.h"
#include "ui/wheel.h"
#include "ui/wheel_geometry.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {

constexpr int kScreenWidth = 1920;
constexpr int kScreenHeight = 1080;
constexpr int kCropWidth = 760;
constexpr int kCropHeight = 640;
constexpr double kSceneTime = 10.0;

enum class Backdrop { Bright, Dark };

struct Scene {
    const char *file;
    int category;
    int hoverItem;
    int hoverCategory;
    double sinceOpen;
    Backdrop backdrop;
};

void SetState(ui::MenuContent &content, ui::MenuItem id, ui::ItemState state,
              std::vector<std::string> details = {}) {
    ui::WheelItem *item = ui::FindItem(content, id);
    item->state = state;
    if (!details.empty()) {
        item->details = std::move(details);
    }
}

void ApplySampleState(ui::MenuContent &content) {
    using ui::ItemState;
    using ui::MenuItem;
    SetState(content, MenuItem::Aimbot, ItemState::On);
    SetState(content, MenuItem::Triggerbot, ItemState::Off);
    SetState(content, MenuItem::NoRecoil, ItemState::On);
    SetState(content, MenuItem::NoSpread, ItemState::Off);
    SetState(content, MenuItem::SilentAngles, ItemState::On);
    SetState(content, MenuItem::VisualNoRecoil, ItemState::On);
    SetState(content, MenuItem::BoneEsp, ItemState::On);
    SetState(content, MenuItem::Bhop, ItemState::On);
    SetState(content, MenuItem::Hooks, ItemState::Ok,
             {"CreateMove 64 per second", "EndScene 144 per second"});
    SetState(content, MenuItem::Interfaces, ItemState::Ok,
             {"RenderView and ModelInfo found", "Fire capture hooks ready"});
    SetState(content, MenuItem::ShotPipeline, ItemState::Ok,
             {"Recoil applied, spread off", "Seed from command 18392"});
    SetState(content, MenuItem::BoneEspStages, ItemState::Fault,
             {"View unavailable", "No OverrideView snapshot in 500 ms"});
}

void PaintBackdrop(preview::Canvas &canvas, Backdrop backdrop) {
    for (int y = 0; y < canvas.height; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(canvas.height);
        for (int x = 0; x < canvas.width; ++x) {
            const float s = static_cast<float>(x) / static_cast<float>(canvas.width);
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            if (backdrop == Backdrop::Bright) {
                const bool sky = t < 0.34f;
                const bool wall = !sky && t < 0.72f &&
                                  std::fmod(s * 7.0f + 0.2f, 1.0f) < 0.78f;
                if (sky) {
                    r = 0.62f + 0.2f * t;
                    g = 0.76f + 0.15f * t;
                    b = 0.88f + 0.05f * t;
                } else if (wall) {
                    const float shade = 0.9f - 0.25f * std::fmod(s * 7.0f, 1.0f);
                    r = 0.78f * shade;
                    g = 0.66f * shade;
                    b = 0.47f * shade;
                } else {
                    r = 0.72f - 0.1f * t;
                    g = 0.60f - 0.1f * t;
                    b = 0.42f - 0.08f * t;
                }
            } else {
                const float glow = std::exp(-8.0f * ((s - 0.7f) * (s - 0.7f) +
                                                     (t - 0.3f) * (t - 0.3f)));
                r = 0.08f + 0.18f * glow;
                g = 0.09f + 0.16f * glow;
                b = 0.10f + 0.10f * glow;
            }
            canvas.Set(x, y, r, g, b);
        }
    }
}

ImVec2 HoverPoint(const ui::MenuContent &content, const Scene &scene,
                  ImVec2 center) {
    namespace geometry = ui::geometry;
    namespace theme = ui::theme;
    const int count = static_cast<int>(content.model.categories.size());
    if (scene.hoverItem >= 0) {
        const auto &items = content.model.categories[scene.category].items;
        const float middle =
            geometry::CategorySector(scene.category, count, theme::kCategoryGap)
                .Middle();
        const float angle =
            geometry::ItemSector(scene.hoverItem, static_cast<int>(items.size()),
                                 middle, theme::kItemSpan, theme::kItemMaxSpan,
                                 theme::kItemGap)
                .Middle();
        const float radius = 0.5f * (theme::kItemInner + theme::kItemOuter);
        return ImVec2(center.x + radius * std::cos(angle),
                      center.y + radius * std::sin(angle));
    }
    if (scene.hoverCategory >= 0) {
        const float angle =
            geometry::CategorySector(scene.hoverCategory, count,
                                     theme::kCategoryGap)
                .Middle();
        const float radius = 0.5f * (theme::kCategoryInner + theme::kCategoryOuter);
        return ImVec2(center.x + radius * std::cos(angle),
                      center.y + radius * std::sin(angle));
    }
    return center;
}

bool Render(const ui::MenuContent &content, const ui::theme::Fonts &fonts,
            const preview::Texture &texture, const Scene &scene,
            const std::string &directory) {
    ImGuiIO &io = ImGui::GetIO();
    const ImVec2 center(0.5f * kScreenWidth, 0.5f * kScreenHeight);
    io.DeltaTime = 1.0f / 60.0f;
    io.MousePos = HoverPoint(content, scene, center);

    ui::WheelState state;
    state.selectedCategory = scene.category;
    state.openedAt = kSceneTime - scene.sinceOpen;
    state.categoryChangedAt = kSceneTime - 1.0;

    ui::WheelInput input;
    input.center = center;
    input.mouse = io.MousePos;
    input.time = kSceneTime;

    ImGui::NewFrame();
    ui::DrawWheel(ImGui::GetForegroundDrawList(), fonts, content.model, state,
                  input);
    ImGui::Render();

    preview::Canvas canvas(kScreenWidth, kScreenHeight);
    PaintBackdrop(canvas, scene.backdrop);
    preview::Rasterize(*ImGui::GetDrawData(), texture, canvas);

    const auto bytes = preview::Crop(
        canvas, static_cast<int>(center.x) - kCropWidth / 2,
        static_cast<int>(center.y) - kCropHeight / 2, kCropWidth, kCropHeight);
    const std::string path = directory + "/" + scene.file;
    const bool written = stbi_write_png(path.c_str(), kCropWidth, kCropHeight,
                                        3, bytes.data(), kCropWidth * 3) != 0;
    std::printf("%s %s\n", written ? "wrote" : "failed", path.c_str());
    return written;
}

} // namespace

int main(int argc, char **argv) {
    const std::string directory = argc > 1 ? argv[1] : ".";

    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(kScreenWidth, kScreenHeight);
    const auto fonts = ui::theme::LoadFonts(io);
    ui::theme::ApplyStyle(ImGui::GetStyle());

    unsigned char *pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(1)));
    const preview::Texture texture{width, height, pixels};

    ui::MenuContent content = ui::BuildMenuContent();
    ApplySampleState(content);

    const Scene scenes[] = {
        {"01-opening.png", 1, -1, -1, 0.07, Backdrop::Bright},
        {"02-accuracy-no-spread.png", 1, 1, -1, 1.0, Backdrop::Bright},
        {"03-status-bone-esp.png", 4, 3, -1, 1.0, Backdrop::Dark},
        {"04-aim-hover-visuals.png", 0, -1, 2, 1.0, Backdrop::Bright},
    };
    bool ok = true;
    for (const auto &scene : scenes) {
        ok = Render(content, fonts, texture, scene, directory) && ok;
    }
    ImGui::DestroyContext();
    return ok ? 0 : 1;
}
