#include "features/menu.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

#include "core/arch.h"
#include "features/bone_esp.h"
#include "features/config.h"
#include "features/telemetry.h"
#include "imgui.h"
#include "ui/menu_content.h"
#include "ui/theme.h"
#include "ui/wheel.h"

namespace features {

namespace {

// A gap longer than this between two drawn frames means the wheel was closed
// in between, so the next frame replays the opening sweep.
constexpr double kReopenGapSeconds = 0.1;

ui::WheelState g_wheel{};
double g_lastDrawn = -1.0;

std::atomic<bool> *ToggleFor(Config &config, ui::MenuItem id) {
    switch (id) {
    case ui::MenuItem::Aimbot:
        return &config.aimbot;
    case ui::MenuItem::Triggerbot:
        return &config.triggerbot;
    case ui::MenuItem::NoRecoil:
        return &config.norecoil;
    case ui::MenuItem::NoSpread:
        return &config.perfectNoSpread;
    case ui::MenuItem::SilentAngles:
        return &config.silentAngles;
    case ui::MenuItem::VisualNoRecoil:
        return &config.visualNoRecoil;
    case ui::MenuItem::BoneEsp:
        return &config.boneEsp;
    case ui::MenuItem::Bhop:
        return &config.bhop;
    default:
        return nullptr;
    }
}

std::string PerSecond(const char *hook, float rate) {
    char text[64];
    std::snprintf(text, sizeof(text), "%s %.0f per second", hook, rate);
    return text;
}

std::string Stage(const char *name, bool requested, bool applied) {
    return std::string(name) +
           (!requested ? " off" : (applied ? " applied" : " unavailable"));
}

void FillHooks(ui::WheelItem &item, const telemetry::HookRates &rates) {
    const float createMove =
        rates.perSecond[static_cast<int>(telemetry::Hook::CreateMove)];
    const float endScene =
        rates.perSecond[static_cast<int>(telemetry::Hook::EndScene)];
    item.state = createMove > 0.0f ? ui::ItemState::Ok : ui::ItemState::Idle;
    item.details = {PerSecond("CreateMove", createMove),
                    PerSecond("EndScene", endScene)};
}

void FillSetup(ui::WheelItem &item) {
    const auto startup = telemetry::GetStartup();
    std::string missing;
    const auto note = [&missing](bool found, const char *name) {
        if (!found) {
            missing += missing.empty() ? name : std::string(", ") + name;
        }
    };
    note(startup.engineTrace, "EngineTrace");
    note(startup.renderView, "RenderView");
    note(startup.modelInfo, "ModelInfo");
    item.state = missing.empty() ? ui::ItemState::Ok : ui::ItemState::Fault;
    item.details = {missing.empty() ? "EngineTrace, RenderView and ModelInfo found"
                                    : "Missing: " + missing};
#if ARCH_X64()
    item.details.push_back(startup.fireCapture && startup.accuracyUpdate
                               ? "Fire-time capture hooks ready"
                               : "Fire-time capture hooks unavailable");
#endif
}

void FillShots(ui::WheelItem &item, bool gameRunning) {
    if (!gameRunning) {
        item.state = ui::ItemState::Idle;
        item.details = {"No commands yet"};
        return;
    }
    const auto trace = telemetry::GetShotTrace();
    const bool recoilMissing = trace.noRecoilRequested && !trace.noRecoilApplied;
    const bool spreadMissing = trace.noSpreadRequested && !trace.noSpreadApplied;
    item.state = recoilMissing || spreadMissing ? ui::ItemState::Fault
                                                : ui::ItemState::Ok;
    item.details = {Stage("Recoil", trace.noRecoilRequested,
                          trace.noRecoilApplied) +
                    ", " +
                    Stage("spread", trace.noSpreadRequested,
                          trace.noSpreadApplied)};
    if (trace.noSpreadApplied) {
        char residual[64];
        std::snprintf(residual, sizeof(residual), "Residual %.4f degrees",
                      trace.spreadResidualDeg);
        item.details.push_back(residual);
    }
}

void FillBoneEsp(ui::WheelItem &item) {
    const auto diagnostics = GetBoneEspDiagnostics();
    if (!diagnostics.enabled) {
        item.state = ui::ItemState::Idle;
        item.details = {"Bone ESP is off"};
        return;
    }
    item.state = ui::ItemState::Fault;
    if (!diagnostics.viewportReady) {
        item.details = {"Viewport unavailable", "IDirect3DDevice9::GetViewport failed"};
    } else if (!diagnostics.viewReady) {
        item.details = {"View unavailable", "No OverrideView snapshot in 500 ms"};
    } else if (!diagnostics.matrixReady) {
        item.details = {"Matrix unavailable", "GetMatricesForView failed its checks"};
    } else if (diagnostics.candidates > 0 && diagnostics.hierarchies == 0) {
        item.details = {"No skeleton resolved", "GetModel or GetStudiomodel failed"};
    } else {
        item.state = ui::ItemState::Ok;
        item.details = {"Viewport, view and matrix ready",
                        std::to_string(diagnostics.projectedLines) +
                            " lines from " +
                            std::to_string(diagnostics.hierarchies) + " enemies"};
    }
}

ui::WheelModel BuildModel(const ui::MenuContent &fixed, Config &config) {
    ui::MenuContent content = fixed;
    for (std::size_t category = 0; category < content.ids.size(); ++category) {
        for (std::size_t item = 0; item < content.ids[category].size(); ++item) {
            const auto *toggle = ToggleFor(config, content.ids[category][item]);
            if (toggle != nullptr) {
                content.model.categories[category].items[item].state =
                    *toggle ? ui::ItemState::On : ui::ItemState::Off;
            }
        }
    }

    const auto rates = telemetry::SampleRates();
    const bool gameRunning =
        rates.perSecond[static_cast<int>(telemetry::Hook::CreateMove)] > 0.0f;
    FillHooks(*ui::FindItem(content, ui::MenuItem::Hooks), rates);
    FillSetup(*ui::FindItem(content, ui::MenuItem::Interfaces));
    FillShots(*ui::FindItem(content, ui::MenuItem::ShotPipeline), gameRunning);
    FillBoneEsp(*ui::FindItem(content, ui::MenuItem::BoneEspStages));
    return content.model;
}

void Activate(const ui::MenuContent &fixed, Config &config,
              const ui::WheelResult &result) {
    if (result.item < 0) {
        return;
    }
    const auto id = fixed.ids[result.category][result.item];
    if (id == ui::MenuItem::PrintDiagnostics) {
        telemetry::RequestDump();
        return;
    }
    if (auto *toggle = ToggleFor(config, id); toggle != nullptr) {
        *toggle = !*toggle;
    }
}

ui::theme::Fonts FontsOrDefault() {
    ui::theme::Fonts fonts = ui::theme::GetFonts();
    ImFont *fallback = ImGui::GetFont();
    fonts.small = fonts.small != nullptr ? fonts.small : fallback;
    fonts.label = fonts.label != nullptr ? fonts.label : fallback;
    fonts.category = fonts.category != nullptr ? fonts.category : fallback;
    fonts.title = fonts.title != nullptr ? fonts.title : fallback;
    return fonts;
}

} // namespace

void Menu() {
    static const ui::MenuContent content = ui::BuildMenuContent();
    auto &config = GetConfig();
    const ImGuiIO &io = ImGui::GetIO();
    const double now = ImGui::GetTime();
    if (g_lastDrawn < 0.0 || now - g_lastDrawn > kReopenGapSeconds) {
        g_wheel.openedAt = now;
        g_wheel.categoryChangedAt = now;
    }
    g_lastDrawn = now;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##wheel", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNav);
    const bool hovered = ImGui::IsWindowHovered();

    ui::WheelInput input;
    input.center = ImVec2(0.5f * io.DisplaySize.x, 0.5f * io.DisplaySize.y);
    input.mouse = io.MousePos;
    input.clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    input.scroll = hovered ? io.MouseWheel : 0.0f;
    input.time = now;

    const auto model = BuildModel(content, config);
    const auto result = ui::DrawWheel(ImGui::GetWindowDrawList(),
                                      FontsOrDefault(), model, g_wheel, input);
    ImGui::End();
    Activate(content, config, result);
}

} // namespace features
