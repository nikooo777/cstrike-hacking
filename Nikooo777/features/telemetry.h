#pragma once

#include "features/perfect_nospread.h"

// Cross-thread status for the menu wheel. Hooks record, the menu reads; every
// accessor is safe from any thread.
namespace features::telemetry {

enum class Hook { CreateMove, OverrideView, EndScene, LockCursor, Count };

void CountCall(Hook hook);

struct HookRates {
    float perSecond[static_cast<int>(Hook::Count)] = {};
};

// Calls per second over the last sampling window. Call once per frame from
// a single thread (the menu does, from EndScene).
HookRates SampleRates();

struct Startup {
    bool renderView = false;
    bool modelInfo = false;
    bool engineTrace = false;
    bool fireCapture = false;
    bool accuracyUpdate = false;
};

void RecordStartup(const Startup &startup);
Startup GetStartup();

void PublishShotTrace(const ShotAngleTrace &trace);
ShotAngleTrace GetShotTrace();

void RequestDump();
bool ConsumeDumpRequest();

} // namespace features::telemetry
