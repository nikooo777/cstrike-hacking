#pragma once

#include <cstdint>

#include "math/vector.h"

class CUserCmd;

namespace game {

// Cone offsets in the right/up plane for pellet 0 (tutorial policy).
struct ConeOffsets {
    float sx = 0.0f;
    float sy = 0.0f;
    bool ok = false;
};

// Result of first-order + iterative inverse-cone compensation.
struct CompensationResult {
    Vector3 angles{};
    // Degrees between intended forward and forward-simulated post-spread dir.
    float forwardErrorDeg = 0.0f;
    int iterations = 0;
    bool ok = false;
};

std::uint8_t Seed8(int randomSeed);

// Source's command seed: MD5_PseudoRandom(command_number) & 0x7fffffff.
std::uint32_t CommandRandomSeed(int commandNumber);

// Prefer a seed already published on the command; otherwise derive the seed
// for a positive command number. Returns false for the zero-sequence temporary
// command used by extra input samples.
bool ResolveCommandRandomSeed(const CUserCmd *userCmd,
                              std::uint32_t &seedOut,
                              bool &fromStoredSeedOut);

// Local UniformRandomStream replay — does not call vstdlib globals, so
// diagnostics stay non-mutating. Matches the Source ran1-style stream used by
// RandomSeed/RandomFloat for this game family.
bool PredictConeOffsets(int randomSeed, float inaccuracy, float spread,
                        ConeOffsets &out);

// Inverse-cone aim: first-order step on the intended basis, then a few
// iterations using the command basis so forward-sim error shrinks.
bool CompensateAngles(const Vector3 &intended, float sx, float sy,
                      CompensationResult &result);

// Forward-simulate post-spread direction from command angles + (sx, sy).
bool ForwardSpreadDirection(const Vector3 &cmdAngles, float sx, float sy,
                            Vector3 &outDir);

// Angular error (degrees) between two unit-ish directions.
float DirectionErrorDegrees(const Vector3 &a, const Vector3 &b);

} // namespace game
