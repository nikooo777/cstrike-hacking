#include "features/bone_esp.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

#include "features/config.h"
#include "core/constants.h"
#include "game/entity_list.h"
#include "game/interfaces.h"
#include "game/model.h"
#include "game/player.h"
#include "game/render_state.h"
#include "imgui.h"
#include "math/projection.h"

namespace features {

namespace {

std::mutex g_diagnosticsMutex;
BoneEspDiagnostics g_diagnostics{};

void SetDiagnostics(const BoneEspDiagnostics &diagnostics) {
    std::lock_guard<std::mutex> lock(g_diagnosticsMutex);
    g_diagnostics = diagnostics;
}

} // namespace

void DrawBoneEsp(float viewportX, float viewportY, float viewportWidth,
                 float viewportHeight) {
    BoneEspDiagnostics diagnostics{};
    diagnostics.enabled = GetConfig().boneEsp;
    if (!diagnostics.enabled) {
        SetDiagnostics(diagnostics);
        return;
    }

    diagnostics.viewportReady =
        std::isfinite(viewportX) && std::isfinite(viewportY) &&
        std::isfinite(viewportWidth) && std::isfinite(viewportHeight) &&
        viewportWidth > 0.0f && viewportHeight > 0.0f;
    if (!diagnostics.viewportReady) {
        SetDiagnostics(diagnostics);
        return;
    }

    CViewSetup viewSetup{};
    diagnostics.viewReady = game::GetCapturedViewSetup(viewSetup);
    if (!diagnostics.viewReady) {
        SetDiagnostics(diagnostics);
        return;
    }

    sdk::render::Matrix4x4 worldToProjection{};
    diagnostics.matrixReady =
        game::GetWorldToProjection(viewSetup, worldToProjection);
    if (!diagnostics.matrixReady) {
        SetDiagnostics(diagnostics);
        return;
    }

    auto *local = game::GetLocalPlayer();
    if (local == nullptr) {
        SetDiagnostics(diagnostics);
        return;
    }

    ImDrawList *drawList = ImGui::GetBackgroundDrawList();
    std::vector<int> parents;
    std::vector<Vector3> positions;
    for (int playerIndex = 1; playerIndex < MAXPLAYERS; ++playerIndex) {
        auto *target = game::GetPlayer(playerIndex);
        if (!game::IsValidTarget(local, target)) {
            continue;
        }
        ++diagnostics.candidates;

        game::BoneHierarchyInfo hierarchy{};
        if (!game::GetBoneParents(target, parents, &hierarchy)) {
            continue;
        }
        ++diagnostics.hierarchies;
        diagnostics.modelAddress = hierarchy.modelAddress;
        diagnostics.studioHeaderAddress = hierarchy.studioHeaderAddress;
        diagnostics.boneCount = hierarchy.boneCount;

        if (!game::GetBonePositions(target, positions)) {
            continue;
        }

        const auto bones = std::min(parents.size(), positions.size());
        for (std::size_t bone = 0; bone < bones; ++bone) {
            const int parent = parents[bone];
            if (parent < 0 || static_cast<std::size_t>(parent) >= bones) {
                continue;
            }

            const Vector3 &bonePosition = positions[bone];
            const Vector3 &parentPosition =
                positions[static_cast<std::size_t>(parent)];
            if (!IsFinite(bonePosition) || !IsFinite(parentPosition)) {
                continue;
            }

            math::ScreenPoint boneScreen{};
            math::ScreenPoint parentScreen{};
            if (!math::WorldToScreen(worldToProjection, bonePosition,
                                     viewportX, viewportY, viewportWidth,
                                     viewportHeight, boneScreen) ||
                !math::WorldToScreen(worldToProjection, parentPosition,
                                     viewportX, viewportY, viewportWidth,
                                     viewportHeight, parentScreen)) {
                continue;
            }

            drawList->AddLine(ImVec2(parentScreen.x, parentScreen.y),
                              ImVec2(boneScreen.x, boneScreen.y),
                              IM_COL32(80, 220, 255, 230), 1.25f);
            ++diagnostics.projectedLines;
        }
    }

    SetDiagnostics(diagnostics);
}

BoneEspDiagnostics GetBoneEspDiagnostics() {
    std::lock_guard<std::mutex> lock(g_diagnosticsMutex);
    return g_diagnostics;
}

} // namespace features
