#include "features/telemetry.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace features::telemetry {

namespace {

constexpr int kHookCount = static_cast<int>(Hook::Count);
constexpr auto kSampleWindow = std::chrono::milliseconds(1000);

std::atomic<std::uint32_t> g_calls[kHookCount] = {};

std::uint32_t g_sampledCalls[kHookCount] = {};
std::chrono::steady_clock::time_point g_sampledAt{};
HookRates g_rates{};

std::mutex g_startupMutex;
Startup g_startup{};

std::mutex g_shotMutex;
ShotAngleTrace g_shot{};

std::atomic<bool> g_dumpRequested{false};

} // namespace

void CountCall(Hook hook) {
    g_calls[static_cast<int>(hook)].fetch_add(1, std::memory_order_relaxed);
}

HookRates SampleRates() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = now - g_sampledAt;
    if (elapsed < kSampleWindow) {
        return g_rates;
    }

    const float seconds = std::chrono::duration<float>(elapsed).count();
    for (int hook = 0; hook < kHookCount; ++hook) {
        const std::uint32_t calls =
            g_calls[hook].load(std::memory_order_relaxed);
        g_rates.perSecond[hook] =
            static_cast<float>(calls - g_sampledCalls[hook]) / seconds;
        g_sampledCalls[hook] = calls;
    }
    g_sampledAt = now;
    return g_rates;
}

void RecordStartup(const Startup &startup) {
    std::lock_guard<std::mutex> lock(g_startupMutex);
    g_startup = startup;
}

Startup GetStartup() {
    std::lock_guard<std::mutex> lock(g_startupMutex);
    return g_startup;
}

void PublishShotTrace(const ShotAngleTrace &trace) {
    std::lock_guard<std::mutex> lock(g_shotMutex);
    g_shot = trace;
}

ShotAngleTrace GetShotTrace() {
    std::lock_guard<std::mutex> lock(g_shotMutex);
    return g_shot;
}

void RequestDump() {
    g_dumpRequested = true;
}

bool ConsumeDumpRequest() {
    return g_dumpRequested.exchange(false);
}

} // namespace features::telemetry
