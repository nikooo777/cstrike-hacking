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

// Inputs for the shared command-angle pipeline. The game applies the current
// punch as +2*punch to the fire angles after CreateMove, so spread must be
// inverted around that post-punch basis rather than around the raw command.
struct ShotAngleRequest {
    Vector3 desiredAngles{};
    Vector3 punchAngles{};
    bool punchReadable = false;
    bool noRecoil = false;
    bool noSpread = false;
    bool spreadAvailable = false;
    float spreadX = 0.0f;
    float spreadY = 0.0f;
};

struct ShotAngleResult {
    Vector3 commandAngles{};
    Vector3 recoilCommandAngles{};
    Vector3 fireBaseAngles{};
    Vector3 spreadAngles{};
    float spreadResidualDeg = 0.0f;
    int spreadIterations = 0;
    bool noRecoilApplied = false;
    bool noSpreadApplied = false;
    bool ok = false;
};

std::uint8_t Seed8(int randomSeed);

// Source's command seed: MD5_PseudoRandom(command_number) & 0x7fffffff.
std::uint32_t CommandRandomSeed(int commandNumber);

bool PredictNextAccuracyPenalty(int shotsFired, float divisor,
                                bool quadratic, float offset, float cap,
                                float currentPenalty, float &out);

// aidocs/005 section 5.6: the movement state selects which weapon-info fields
// and which decay constant the slot-384 penalty decay uses.
enum class PenaltyBaseline { Stand, Crouch, StandPlusLadder };
enum class PenaltyRecovery { Stand, Crouch };

inline constexpr float kGroundPenaltyDecay = -2.3025851f;
inline constexpr float kAirbornePenaltyDecay = -0.7675284f;

struct PenaltyDecayRule {
    PenaltyBaseline baseline = PenaltyBaseline::Stand;
    PenaltyRecovery recovery = PenaltyRecovery::Stand;
    float decayConstant = kGroundPenaltyDecay;
};

PenaltyDecayRule SelectPenaltyDecayRule(bool onLadder, bool onGround,
                                        bool ducking);

bool PredictAccuracyPenaltyDecay(float currentPenalty, float baseline,
                                 float recoveryTime, float intervalPerTick,
                                 float decayConstant, float &out);

bool PredictCssPunchDecay(const Vector3 &currentPunch,
                          float intervalPerTick, Vector3 &out);

// Prefer a seed already published on the command; otherwise derive the seed
// for a positive command number. Returns false for the zero-sequence temporary
// command used by extra input samples.
bool ResolveCommandRandomSeed(const CUserCmd *userCmd,
                              std::uint32_t &seedOut,
                              bool &fromStoredSeedOut);

// Replay this build's CS FX_FireBullets cone without touching vstdlib's global
// stream. The two inputs are separate radii: GetInaccuracy() is sampled once
// per shot and GetSpread() is sampled once per pellet.
bool PredictConeOffsets(int randomSeed, float inaccuracy, float spread,
                        ConeOffsets &out);

// Inverse-cone aim: first-order step on the intended basis, then a few
// iterations using the command basis so forward-sim error shrinks.
bool CompensateAngles(const Vector3 &intended, float sx, float sy,
                      CompensationResult &result);

// Compose aim, recoil, and spread in the same order as the game. This is pure
// math: feature code supplies the live punch/seed-derived cone state and the
// config-selected booleans, while this function keeps the toggle interactions
// testable without loading the game.
bool ComposeShotAngles(const ShotAngleRequest &request,
                       ShotAngleResult &result);

// Forward-simulate post-spread direction from command angles + (sx, sy).
bool ForwardSpreadDirection(const Vector3 &cmdAngles, float sx, float sy,
                            Vector3 &outDir);

// Angular error (degrees) between two unit-ish directions.
float DirectionErrorDegrees(const Vector3 &a, const Vector3 &b);

} // namespace game
