#include "game/render_state.h"

#include <chrono>
#include <mutex>

namespace game {

namespace {

constexpr auto kMaxViewSetupAge = std::chrono::milliseconds(500);

std::mutex g_viewSetupMutex;
CViewSetup g_viewSetup{};
std::chrono::steady_clock::time_point g_viewSetupCapturedAt{};
bool g_viewSetupValid = false;

} // namespace

void CaptureViewSetup(const CViewSetup &viewSetup) {
    std::lock_guard<std::mutex> lock(g_viewSetupMutex);
    g_viewSetup = viewSetup;
    g_viewSetupCapturedAt = std::chrono::steady_clock::now();
    g_viewSetupValid = true;
}

bool GetCapturedViewSetup(CViewSetup &viewSetup) {
    std::lock_guard<std::mutex> lock(g_viewSetupMutex);
    if (!g_viewSetupValid ||
        std::chrono::steady_clock::now() - g_viewSetupCapturedAt >
            kMaxViewSetupAge) {
        return false;
    }

    viewSetup = g_viewSetup;
    return true;
}

} // namespace game
