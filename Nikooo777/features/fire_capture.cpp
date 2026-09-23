#include "features/fire_capture.h"

#include <cmath>
#include <cstdint>
#include <iostream>

#include "core/arch.h"
#include "core/constants.h"
#include "features/norecoil.h"
#include "features/perfect_nospread.h"
#include "game/entity_list.h"
#include "game/player.h"
#include "game/timing.h"
#include "game/weapon.h"
#include "game/weapon_math.h"
#include "memory/mem.h"
#include "netvars/netvars.h"
#include "sdk/client_offsets.h"
#include "sdk/user_cmd.h"

namespace features {

#if ARCH_X64()
namespace {

struct ClientFireDiagnosticState {
    game::WeaponEntity weapon = nullptr;
    Vector3 targetAngles{};
    Vector3 commandAngles{};
    Vector3 expectedFireAngles{};
    Vector3 currentPunchAngles{};
    Vector3 predictedPunchAngles{};
    game::ConeOffsets predictedOffsets{};
    std::uint32_t expectedSeed = 0;
    float getterInaccuracy = 0.0f;
    float predictedFireInaccuracy = 0.0f;
    float getterSpread = 0.0f;
    float decayedAccuracyPenalty = 0.0f;
    float accuracyBaseline = 0.0f;
    float accuracyRecoveryTime = 0.0f;
    float intervalPerTick = 0.0f;
    float currentAccuracyPenalty = 0.0f;
    float updatePenaltyBefore = 0.0f;
    float updatePenaltyAfter = 0.0f;
    float updateIntervalBefore = 0.0f;
    float updateIntervalAfter = 0.0f;
    float currentSpeed2D = 0.0f;
    int currentFlags = 0;
    std::uint8_t currentMoveType = 0;
    int commandNumber = 0;
    int weaponId = -1;
    int weaponInfoIndex = -1;
    int mode = 0;
    int accuracyModel = -1;
    int penaltyUpdateCalls = 0;
    bool armed = false;
    bool commandCaptured = false;
    bool seedOk = false;
    bool fireAnglesOk = false;
    bool weaponIdOk = false;
    bool weaponInfoIndexOk = false;
    bool modeOk = false;
    bool accuracyModelOk = false;
    bool radiiOk = false;
    bool preFireDecayOk = false;
    bool predictedPunchOk = false;
    bool currentMotionOk = false;
    bool currentPenaltyOk = false;
    bool updatePenaltyBeforeOk = false;
    bool updatePenaltyAfterOk = false;
    bool updateIntervalBeforeOk = false;
    bool updateIntervalAfterOk = false;
};

ClientFireDiagnosticState g_clientFireDiagnostic{};

bool ReadPlayerMotion(const CCSPlayer *player, float &speed2D, int &flags,
                      std::uint8_t &moveType) {
    speed2D = 0.0f;
    flags = 0;
    moveType = 0;
    if (player == nullptr) {
        return false;
    }

    const int velocityOffset = netvars::GetOffset(
        "DT_LocalPlayerExclusive", "m_vecVelocity");
    const int flagsOffset =
        netvars::GetOffset("DT_BasePlayer", "m_fFlags");
    if (velocityOffset < 0 || flagsOffset < 0) {
        return false;
    }

    const auto *bytes = reinterpret_cast<const char *>(player);
    Vector3 velocity{};
    if (!mem::ReadValue(bytes + velocityOffset, velocity) ||
        !mem::ReadValue(bytes + flagsOffset, flags)) {
        return false;
    }
    if (!mem::ReadValue(bytes + sdk::offsets::kMoveType, moveType)) {
        return false;
    }

    speed2D = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);
    return std::isfinite(speed2D);
}

bool ReadWeaponPenalty(game::WeaponEntity weapon, float &penalty) {
    penalty = 0.0f;
    if (weapon == nullptr) {
        return false;
    }

    const int penaltyOffset =
        netvars::GetOffset("DT_WeaponCSBase", "m_fAccuracyPenalty");
    return penaltyOffset >= 0 &&
           mem::ReadValue(reinterpret_cast<const char *>(weapon) +
                              penaltyOffset,
                          penalty) &&
           std::isfinite(penalty);
}

bool ReadActiveWeaponPenalty(const CCSPlayer *player, float &penalty) {
    game::WeaponEntity weapon = nullptr;
    return game::GetActiveWeapon(player, weapon) &&
           ReadWeaponPenalty(weapon, penalty);
}

} // namespace

void ArmClientFireDiagnostic() {
    g_clientFireDiagnostic = {};
    g_clientFireDiagnostic.armed = true;
    std::cout << "Client fire-time diagnostic armed" << std::endl;
}

void CaptureClientFireCommand(const CUserCmd *userCmd,
                              const ShotAngleTrace &shotTrace) {
    if (!g_clientFireDiagnostic.armed || userCmd == nullptr ||
        userCmd->command_number <= 0 ||
        (userCmd->buttons & IN_ATTACK) == 0) {
        return;
    }

    g_clientFireDiagnostic = {};
    auto &capture = g_clientFireDiagnostic;
    capture.armed = true;
    capture.commandNumber = userCmd->command_number;
    capture.commandAngles = userCmd->viewangles;

    bool fromStoredSeed = false;
    capture.seedOk = game::ResolveCommandRandomSeed(
        userCmd, capture.expectedSeed, fromStoredSeed);

    capture.targetAngles = shotTrace.fireBaseAngles;
    capture.fireAnglesOk = shotTrace.recoilStateReadable;
    capture.currentPunchAngles = shotTrace.currentPunchAngles;
    capture.predictedPunchAngles = shotTrace.punchAngles;
    capture.predictedPunchOk = shotTrace.firePunchPredicted;
    capture.intervalPerTick = shotTrace.intervalPerTick;
    if (capture.fireAnglesOk) {
        capture.expectedFireAngles =
            userCmd->viewangles + shotTrace.punchAngles * 2.0f;
    }

    auto *local = game::GetLocalPlayer();
    game::WeaponSpreadState spreadState{};
    if (local != nullptr && game::ReadWeaponSpreadState(local, spreadState)) {
        capture.weapon = spreadState.weapon;
        capture.weaponId = spreadState.weaponId;
        capture.weaponIdOk = spreadState.weaponIdOk;
        capture.weaponInfoIndex = spreadState.weaponInfoIndex;
        capture.weaponInfoIndexOk = spreadState.weaponInfoIndexOk;
        capture.mode = spreadState.mode;
        capture.modeOk = spreadState.modeOk;
        capture.accuracyModel = spreadState.accuracyModel;
        capture.accuracyModelOk = spreadState.accuracyModelOk;
        capture.getterInaccuracy = spreadState.inaccuracy;
        capture.predictedFireInaccuracy = spreadState.fireInaccuracy;
        capture.getterSpread = spreadState.spread;
        capture.currentAccuracyPenalty = spreadState.accuracyPenalty;
        capture.currentPenaltyOk = spreadState.penaltyOk;
        capture.decayedAccuracyPenalty =
            spreadState.decayedAccuracyPenalty;
        capture.accuracyBaseline = spreadState.accuracyBaseline;
        capture.accuracyRecoveryTime =
            spreadState.accuracyRecoveryTime;
        capture.preFireDecayOk = spreadState.preFireDecayOk;
        capture.currentMotionOk = ReadPlayerMotion(
            local, capture.currentSpeed2D, capture.currentFlags,
            capture.currentMoveType);
        capture.radiiOk =
            spreadState.fireInaccuracyOk && spreadState.spreadOk;
        if (capture.seedOk && capture.radiiOk) {
            game::PredictConeOffsets(
                static_cast<int>(capture.expectedSeed),
                capture.predictedFireInaccuracy, capture.getterSpread,
                capture.predictedOffsets);
        }
    }

    capture.commandCaptured = true;
}

void RecordAccuracyPenaltyUpdate(void *weapon, bool beforeCall) {
    auto &capture = g_clientFireDiagnostic;
    if (!capture.armed || !capture.commandCaptured || weapon == nullptr ||
        weapon != capture.weapon) {
        return;
    }

    if (beforeCall) {
        ++capture.penaltyUpdateCalls;
        capture.updatePenaltyBeforeOk =
            ReadWeaponPenalty(weapon, capture.updatePenaltyBefore);
        capture.updateIntervalBeforeOk =
            game::GetIntervalPerTick(capture.updateIntervalBefore);
        return;
    }

    capture.updatePenaltyAfterOk =
        ReadWeaponPenalty(weapon, capture.updatePenaltyAfter);
    capture.updateIntervalAfterOk =
        game::GetIntervalPerTick(capture.updateIntervalAfter);
}

void RecordClientFireBullets(int playerIndex, const Vector3 *origin,
                             const Vector3 *fireAngles, int weaponId,
                             int mode, int seed, float inaccuracy,
                             float spread, float soundTime) {
    auto &capture = g_clientFireDiagnostic;
    if (!capture.armed || !capture.commandCaptured) {
        return;
    }

    const auto actualSeed8 = static_cast<std::uint8_t>(seed & 0xff);
    if ((capture.seedOk &&
         actualSeed8 != game::Seed8(static_cast<int>(capture.expectedSeed))) ||
        (capture.weaponIdOk && weaponId != capture.weaponId) ||
        (capture.modeOk && mode != capture.mode)) {
        return;
    }

    Vector3 actualOrigin{};
    const bool originOk = origin != nullptr && mem::ReadValue(origin, actualOrigin);
    Vector3 actualFireAngles{};
    const bool actualFireAnglesOk =
        fireAngles != nullptr && mem::ReadValue(fireAngles, actualFireAngles);

    std::cout << "client fire-time diag:" << std::endl;
    std::cout << "  player=" << playerIndex
              << " weaponId=" << weaponId
              << " weaponInfoIndex=";
    if (capture.weaponInfoIndexOk) {
        std::cout << capture.weaponInfoIndex;
    } else {
        std::cout << "unavailable";
    }
    std::cout
              << " mode=" << mode
              << " command=" << capture.commandNumber
              << " accuracy_model=";
    if (capture.accuracyModelOk) {
        std::cout << capture.accuracyModel;
    } else {
        std::cout << "unavailable";
    }
    std::cout << std::endl;
    std::cout << "  seed expected8=";
    if (capture.seedOk) {
        std::cout << "0x" << std::hex
                  << static_cast<unsigned>(game::Seed8(
                         static_cast<int>(capture.expectedSeed)));
    } else {
        std::cout << "unavailable";
    }
    std::cout << " actual8=0x" << std::hex
              << static_cast<unsigned>(actualSeed8) << std::dec
              << " match="
              << (capture.seedOk &&
                          actualSeed8 == game::Seed8(
                                             static_cast<int>(
                                                 capture.expectedSeed))
                      ? "yes"
                      : "unavailable")
              << std::endl;
    std::cout << "  radii create_move_getter=(";
    if (capture.radiiOk) {
        std::cout << capture.getterInaccuracy << ","
                  << capture.getterSpread;
    } else {
        std::cout << "unavailable";
    }
    std::cout << ") predicted_fire_inaccuracy="
              << capture.predictedFireInaccuracy
              << " actual=(" << inaccuracy << "," << spread << ")"
              << " delta=(" << (inaccuracy - capture.getterInaccuracy)
              << "," << (spread - capture.getterSpread) << ")"
              << " predicted_delta=("
              << (inaccuracy - capture.predictedFireInaccuracy) << ","
              << (spread - capture.getterSpread) << ")"
              << std::endl;
    std::cout << "  accuracy decay=";
    if (capture.preFireDecayOk) {
        std::cout << capture.decayedAccuracyPenalty
                  << " baseline=" << capture.accuracyBaseline
                  << " recovery=" << capture.accuracyRecoveryTime
                  << " interval=" << capture.intervalPerTick;
    } else {
        std::cout << "unavailable";
    }
    std::cout << std::endl;
    std::cout << "  angles cmd=(" << capture.commandAngles.x << ","
              << capture.commandAngles.y << ") expected_fire=";
    if (capture.fireAnglesOk) {
        std::cout << "(" << capture.expectedFireAngles.x << ","
                  << capture.expectedFireAngles.y << ")";
    } else {
        std::cout << "unavailable";
    }
    std::cout << " actual_fire=";
    if (actualFireAnglesOk) {
        std::cout << "(" << actualFireAngles.x << ","
                  << actualFireAngles.y << "," << actualFireAngles.z << ")";
    } else {
        std::cout << "unavailable";
    }
    if (capture.fireAnglesOk && actualFireAnglesOk) {
        std::cout << " delta=("
                  << (actualFireAngles.x - capture.expectedFireAngles.x)
                  << ","
                  << (actualFireAngles.y - capture.expectedFireAngles.y)
                  << ")";
    }
    std::cout << std::endl;
    std::cout << "  update_accuracy calls="
              << capture.penaltyUpdateCalls << " penalty_before=";
    if (capture.updatePenaltyBeforeOk) {
        std::cout << capture.updatePenaltyBefore;
    } else {
        std::cout << "unavailable";
    }
    std::cout << " penalty_after=";
    if (capture.updatePenaltyAfterOk) {
        std::cout << capture.updatePenaltyAfter;
    } else {
        std::cout << "unavailable";
    }
    std::cout << " interval_before=";
    if (capture.updateIntervalBeforeOk) {
        std::cout << capture.updateIntervalBefore;
    } else {
        std::cout << "unavailable";
    }
    std::cout << " interval_after=";
    if (capture.updateIntervalAfterOk) {
        std::cout << capture.updateIntervalAfter;
    } else {
        std::cout << "unavailable";
    }
    std::cout << std::endl;
    auto *local = game::GetLocalPlayer();
    float actualPenalty = 0.0f;
    const bool actualPenaltyOk =
        local != nullptr && ReadActiveWeaponPenalty(local, actualPenalty);
    std::cout << "  inaccuracy components create_penalty=";
    if (capture.currentPenaltyOk) {
        std::cout << capture.currentAccuracyPenalty
                  << " create_base="
                  << (capture.getterInaccuracy -
                      capture.currentAccuracyPenalty);
    } else {
        std::cout << "unavailable create_base=unavailable";
    }
    std::cout << " actual_penalty=";
    if (actualPenaltyOk) {
        std::cout << actualPenalty
                  << " actual_base=" << (inaccuracy - actualPenalty);
    } else {
        std::cout << "unavailable actual_base=unavailable";
    }
    std::cout << std::endl;

    float actualSpeed2D = 0.0f;
    int actualFlags = 0;
    std::uint8_t actualMoveType = 0;
    const bool actualMotionOk =
        local != nullptr && ReadPlayerMotion(
                                local, actualSpeed2D, actualFlags,
                                actualMoveType);
    std::cout << "  movement create=";
    if (capture.currentMotionOk) {
        std::cout << "speed2d=" << capture.currentSpeed2D
                  << " flags=0x" << std::hex << capture.currentFlags
                  << " movetype=0x"
                  << static_cast<unsigned>(capture.currentMoveType)
                  << std::dec;
    } else {
        std::cout << "unavailable";
    }
    std::cout << " actual=";
    if (actualMotionOk) {
        std::cout << "speed2d=" << actualSpeed2D
                  << " flags=0x" << std::hex << actualFlags
                  << " movetype=0x"
                  << static_cast<unsigned>(actualMoveType) << std::dec;
    } else {
        std::cout << "unavailable";
    }
    std::cout << std::endl;

    RecoilState actualRecoil{};
    Vector3 actualSource{};
    const bool actualRecoilOk =
        local != nullptr && ReadRecoilState(local, actualRecoil);
    const bool actualSourceOk =
        local != nullptr && game::GetLocalEyeAngles(local, actualSource);
    std::cout << "  punch create=(" << capture.currentPunchAngles.x << ","
              << capture.currentPunchAngles.y << ") predicted_fire=";
    if (capture.predictedPunchOk) {
        std::cout << "(" << capture.predictedPunchAngles.x << ","
                  << capture.predictedPunchAngles.y << ")";
    } else {
        std::cout << "unavailable";
    }
    std::cout << " actual=";
    if (actualRecoilOk) {
        std::cout << "(" << actualRecoil.punchAngles.x << ","
                  << actualRecoil.punchAngles.y << ")";
    } else {
        std::cout << "unavailable";
    }
    std::cout << std::endl;
    std::cout << "  fire source=";
    if (actualSourceOk) {
        std::cout << "(" << actualSource.x << "," << actualSource.y << ")";
    } else {
        std::cout << "unavailable";
    }
    if (actualSourceOk && actualRecoilOk && actualFireAnglesOk) {
        const Vector3 reconstructed =
            actualSource + actualRecoil.punchAngles * 2.0f;
        std::cout << " reconstructed=(" << reconstructed.x << ","
                  << reconstructed.y << ") delta=("
                  << (actualFireAngles.x - reconstructed.x) << ","
                  << (actualFireAngles.y - reconstructed.y) << ")";
    }
    std::cout << std::endl;
    std::cout << "  origin=";
    if (originOk) {
        std::cout << "(" << actualOrigin.x << "," << actualOrigin.y << ","
                  << actualOrigin.z << ")";
    } else {
        std::cout << "unavailable";
    }
    std::cout << " sound_time=" << soundTime << std::endl;

    game::ConeOffsets actualOffsets{};
    if (game::PredictConeOffsets(seed, inaccuracy, spread, actualOffsets)) {
        std::cout << "  offsets create_move=(";
        if (capture.predictedOffsets.ok) {
            std::cout << capture.predictedOffsets.sx << ","
                      << capture.predictedOffsets.sy;
        } else {
            std::cout << "unavailable";
        }
        std::cout << ") actual_args_replay=(" << actualOffsets.sx << ","
                  << actualOffsets.sy << ")" << std::endl;

        Vector3 actualDirection{};
        Vector3 targetDirection{};
        if (actualFireAnglesOk &&
            game::ForwardSpreadDirection(
                actualFireAngles, actualOffsets.sx, actualOffsets.sy,
                actualDirection) &&
            game::ForwardSpreadDirection(
                capture.targetAngles, 0.0f, 0.0f, targetDirection)) {
            std::cout << "  final direction target=("
                      << capture.targetAngles.x << ","
                      << capture.targetAngles.y << ") residual_deg="
                      << game::DirectionErrorDegrees(
                             actualDirection, targetDirection)
                      << std::endl;
        }
    }

    capture = {};
}

const char *ClientFireCaptureStatus() {
    if (!g_clientFireDiagnostic.armed) {
        return "idle";
    }
    return g_clientFireDiagnostic.commandCaptured
               ? "waiting_for_matching_fire_call"
               : "waiting_for_attack_command";
}
#else
const char *ClientFireCaptureStatus() {
    return "idle";
}
#endif

} // namespace features
