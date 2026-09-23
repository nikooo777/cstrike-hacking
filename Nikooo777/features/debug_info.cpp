#include "features/debug_info.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

#include "core/constants.h"
#include "core/modules.h"
#include "features/config.h"
#include "features/bone_esp.h"
#include "features/norecoil.h"
#include "features/perfect_nospread.h"
#include "game/entity_list.h"
#include "game/interfaces.h"
#include "game/player.h"
#include "game/timing.h"
#include "game/weapon.h"
#include "game/weapon_math.h"
#include "memory/mem.h"
#include "netvars/netvars.h"
#include "sdk/engine_client.h"
#include "sdk/user_cmd.h"

namespace features {

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

void PrintNetvar(const char *label, const char *table, const char *property) {
    const int offset = netvars::GetOffset(table, property);
    std::cout << label << ": ";
    if (offset >= 0) {
        std::cout << "0x" << std::hex << offset << std::dec;
    } else {
        std::cout << "missing";
    }
    std::cout << " (" << table << "->" << property << ")" << std::endl;
}

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
        !mem::ReadValue(bytes + flagsOffset, flags) ||
        !mem::ReadValue(bytes + 0x1f4, moveType)) {
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

void CaptureClientFireCommand(const CUserCmd *userCmd) {
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

    const auto &shotTrace = GetLastShotAngleTrace();
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

void PrintDebugInfo(const CUserCmd *userCmd) {
    std::cout << "clientModuleBase: 0x" << std::hex << core::GetModule("client.dll") << std::endl;
    std::cout << "serverModuleBase: 0x" << std::hex << core::GetModule("server.dll") << std::endl;
    std::cout << "engineModuleBase: 0x" << std::hex << core::GetModule("engine.dll") << std::endl;

    std::cout << "runtime interfaces/client-only offsets:" << std::endl;
    std::cout << "  clientEntityList: 0x" << std::hex
              << game::GetClientEntityList() << std::dec << std::endl;
    PrintNetvar("entity m_lifeState", "DT_BasePlayer", "m_lifeState");
    PrintNetvar("entity m_iHealth", "DT_BasePlayer", "m_iHealth");
    PrintNetvar("entity m_iTeamNum", "DT_BaseEntity", "m_iTeamNum");
    PrintNetvar("entity m_vecOrigin", "DT_BaseEntity", "m_vecOrigin");
    PrintNetvar("entity m_vecViewOffset", "DT_LocalPlayerExclusive", "m_vecViewOffset");
    PrintNetvar("entity m_vecVelocity", "DT_LocalPlayerExclusive", "m_vecVelocity");
    PrintNetvar("entity m_vecBaseVelocity", "DT_LocalPlayerExclusive", "m_vecBaseVelocity");
    PrintNetvar("entity m_angRotation", "DT_BaseEntity", "m_angRotation");
    PrintNetvar("entity m_fFlags", "DT_BasePlayer", "m_fFlags");
    PrintNetvar("entity m_Local", "DT_LocalPlayerExclusive", "m_Local");
    PrintNetvar("local m_vecPunchAngle", "DT_Local", "m_vecPunchAngle");
    PrintNetvar("local m_vecPunchAngleVel", "DT_Local", "m_vecPunchAngleVel");
    PrintNetvar("player m_iShotsFired", "DT_CSLocalPlayerExclusive", "m_iShotsFired");
    PrintNetvar("weapon m_hActiveWeapon", "DT_BaseCombatCharacter", "m_hActiveWeapon");
    PrintNetvar("weapon m_weaponMode", "DT_WeaponCSBase", "m_weaponMode");
    PrintNetvar("weapon m_fAccuracyPenalty", "DT_WeaponCSBase", "m_fAccuracyPenalty");
    PrintNetvar("weapon m_iClip1", "DT_LocalWeaponData", "m_iClip1");
#if defined(_WIN32) && !defined(_WIN64)
    std::cout << "client-only m_iCrosshairID: 0x" << std::hex << 0x14F0 << std::dec << std::endl;
#else
    std::cout << "client-only m_iIDEntIndex (crosshair target): 0x"
              << std::hex << 0x1B20 << std::dec
              << " (C_CSPlayer::GetIDTarget)" << std::endl;
#endif
    const auto clientStateAddress = game::GetClientStateAddress();
    const auto engineBase = core::GetModule("engine.dll");
    std::cout << "clientState addr: 0x" << std::hex << clientStateAddress << std::endl;
    if (clientStateAddress != 0 && engineBase != 0 && clientStateAddress >= engineBase) {
        std::cout << "clientState offset: engine.dll + 0x" << std::hex
                  << clientStateAddress - engineBase << std::dec << std::endl;
    } else {
        std::cout << "clientState offset: unavailable" << std::endl;
    }
#if defined(_WIN32) && !defined(_WIN64)
    std::cout << "ViewAngles: clientState + 0x4b84" << std::endl;
#else
    Vector3 viewAngles{};
    if (game::GetViewAngles(viewAngles)) {
        std::cout << "ViewAngles: VEngineClient014::GetViewAngles slot "
                  << std::dec << kEngineClientGetViewAnglesVtableIndex << " -> "
                  << viewAngles << std::endl;
    } else {
        std::cout << "ViewAngles: VEngineClient014::GetViewAngles unavailable"
                  << std::endl;
    }
#endif

    auto *local = game::GetLocalPlayer();
    if (local != nullptr) {
        game::DormancyInfo dormancy;
        if (game::GetDormancyInfo(local, dormancy)) {
            std::cout << "local dormant: " << (dormancy.dormant ? "yes" : "no")
                      << " (interface resolved)" << std::endl;
        } else {
            std::cout << "local dormant: unavailable (interface unresolved)"
                      << std::endl;
        }

        int validTargets = 0;
        int readableTargetBones = 0;
        int visibleTargets = 0;
        for (int playerIndex = 1; playerIndex < MAXPLAYERS; ++playerIndex) {
            auto *target = game::GetPlayer(playerIndex);
            if (!game::IsValidTarget(local, target)) {
                continue;
            }

            ++validTargets;
            Vector3 headPosition{};
            if (!game::GetBonePosition(target, 14, headPosition)) {
                continue;
            }
            ++readableTargetBones;
            if (game::IsVisible(local, target, headPosition)) {
                ++visibleTargets;
            }
        }
        std::cout << "aimbot diagnostics: enabled="
                  << (features::GetConfig().aimbot ? "yes" : "no")
                  << " validTargets=" << validTargets
                  << " readableBone14=" << readableTargetBones
                  << " visibleTargets=" << visibleTargets << std::endl;

        game::BoneCacheInfo boneCache;
        const bool boneCacheUsable = game::GetBoneCacheInfo(local, boneCache);
        std::cout << "bone cache: entity=0x" << std::hex
                  << boneCache.entityAddress << " matrix=0x" << boneCache.matrix
                  << " count=" << std::dec << boneCache.count
                  << " matrixReadable=" << (boneCache.matrixReadable ? "yes" : "no")
                  << " countReadable=" << (boneCache.countReadable ? "yes" : "no")
                  << " usable=" << (boneCacheUsable ? "yes" : "no")
                  << " (entity + 0x" << std::hex
                  << CBasePlayer::kBoneMatrixOffset << ", count + 0x"
                  << CBasePlayer::kBoneCountOffset << ", "
                  << "C_BaseAnimating::m_CachedBoneData)"
                  << std::dec << std::endl;

        const auto boneEsp = features::GetBoneEspDiagnostics();
        std::cout << "bone esp: enabled="
                  << (boneEsp.enabled ? "yes" : "no")
                  << " viewport="
                  << (boneEsp.viewportReady ? "ready" : "unavailable")
                  << " view=" << (boneEsp.viewReady ? "ready" : "unavailable")
                  << " matrix="
                  << (boneEsp.matrixReady ? "ready" : "unavailable")
                  << " candidates=" << boneEsp.candidates
                  << " hierarchies=" << boneEsp.hierarchies
                  << " projectedLines=" << boneEsp.projectedLines
                  << " model=0x" << std::hex << boneEsp.modelAddress
                  << " studio=0x" << boneEsp.studioHeaderAddress << std::dec
                  << " bones=" << boneEsp.boneCount << std::endl;

        std::cout << "my position:" << local->m_vecOrigin() << std::endl;

        if (userCmd != nullptr) {
            // Weapon resolution and virtual GetInaccuracy/GetSpread calls must
            // happen on the game thread. PrintDebugInfo() is also called once
            // during injection setup, where userCmd is null and the caller is
            // the loader thread; do not touch live weapon objects there.
            game::WeaponSpreadState spreadState{};
            const bool spreadOk = game::ReadWeaponSpreadState(local, spreadState);
            std::cout << "spread diag: weapon=";
            if (!spreadState.weaponResolved) {
                std::cout << "unresolved handle=";
                if (spreadState.handleReadOk) {
                    std::cout << "0x" << std::hex << spreadState.handle
                              << std::dec << " index=0x" << std::hex
                              << (spreadState.handle & 0xFFFFu) << std::dec;
                } else {
                    std::cout << "unread";
                }
            } else {
                std::cout << "0x" << std::hex
                          << reinterpret_cast<std::uintptr_t>(spreadState.weapon)
                          << std::dec << " handle=0x" << std::hex << spreadState.handle
                          << std::dec;
            }
            std::cout << std::endl;
            std::cout << "  accuracy dispatch: inaccuracy_method=0x" << std::hex
                      << spreadState.inaccuracyMethod
                      << " spread_method=0x" << spreadState.spreadMethod
                      << std::dec << std::endl;
            std::cout << "  accuracy model: weapon_accuracy_model parent_slot=0x"
                      << std::hex << spreadState.accuracyModelParentSlot
                      << " convar=0x" << spreadState.accuracyModelConVar
                      << std::dec << " value=";
            if (spreadState.accuracyModelOk) {
                std::cout << spreadState.accuracyModel;
            } else {
                std::cout << "unavailable";
            }
            std::cout << " weapon+0xca8=";
            if (spreadState.accuracyStateOk) {
                std::cout << spreadState.accuracyState;
            } else {
                std::cout << "unavailable";
            }
            std::cout << " (1=unmodeled, other=modeled)" << std::endl;
            std::cout << "  mode="
                      << (spreadState.modeOk ? std::to_string(spreadState.mode)
                                             : std::string("unread"))
                      << " weaponId="
                      << (spreadState.weaponIdOk
                              ? std::to_string(spreadState.weaponId)
                              : std::string("unread"))
                      << " weaponInfoIndex="
                      << (spreadState.weaponInfoIndexOk
                              ? std::to_string(
                                    spreadState.weaponInfoIndex)
                              : std::string("unread"))
                      << " shotsFired="
                      << (spreadState.shotsFiredOk
                              ? std::to_string(spreadState.shotsFired)
                              : std::string("unread"))
                      << " clip="
                      << (spreadState.clipOk ? std::to_string(spreadState.clip1)
                                             : std::string("unread"))
                      << " penalty="
                      << (spreadState.penaltyOk
                              ? std::to_string(spreadState.accuracyPenalty)
                              : std::string("unread"))
                      << " nextPenalty="
                      << (spreadState.nextPenaltyOk
                              ? std::to_string(spreadState.fireAccuracyPenalty)
                              : std::string("unavailable"))
                      << std::endl;
            std::cout << "  pre-fire decay=";
            if (spreadState.preFireDecayOk) {
                std::cout << spreadState.decayedAccuracyPenalty
                          << " baseline=" << spreadState.accuracyBaseline
                          << " recovery="
                          << spreadState.accuracyRecoveryTime
                          << " interval=" << spreadState.intervalPerTick;
            } else {
                std::cout << "unavailable";
            }
            std::cout << std::endl;
            std::cout << "  GetInaccuracy=";
            if (spreadState.inaccuracyOk) {
                std::cout << spreadState.inaccuracy;
            } else {
                std::cout << "fail";
            }
            std::cout << " fireInaccuracy=";
            if (spreadState.fireInaccuracyOk) {
                std::cout << spreadState.fireInaccuracy;
            } else {
                std::cout << "unavailable";
            }
            std::cout << " GetSpread=";
            if (spreadState.spreadOk) {
                std::cout << spreadState.spread;
            } else {
                std::cout << "fail";
            }
            std::cout << " methodsOk=" << (spreadOk ? "yes" : "no")
                      << " fireRadii=";
            if (spreadState.fireInaccuracyOk && spreadState.spreadOk) {
                std::cout << "(inaccuracy=" << spreadState.fireInaccuracy
                          << ",spread=" << spreadState.spread << ")";
            } else {
                std::cout << "unavailable";
            }
            std::cout << " (polar seed8+1)"
                      << " next_accuracy="
                      << (spreadState.nextPenaltyOk ? "yes" : "no")
                      << " usableForCompensation="
                      << (spreadState.usableForCompensation ? "yes" : "no")
                      << std::endl;

            const auto storedSeed = userCmd->random_seed;
            std::uint32_t effectiveSeed = 0;
            bool fromStoredSeed = false;
            const bool seedOk = game::ResolveCommandRandomSeed(
                userCmd, effectiveSeed, fromStoredSeed);
            std::cout << "  cmd.ptr=0x" << std::hex
                      << reinterpret_cast<std::uintptr_t>(userCmd)
                      << " cmd.number=" << std::dec << userCmd->command_number
                      << " tick=" << userCmd->tick_count
                      << " stored_seed=0x" << std::hex << storedSeed
                      << " effective_seed=";
            if (seedOk) {
                std::cout << "0x" << effectiveSeed << " seed_source="
                          << (fromStoredSeed ? "stored" : "command_number")
                          << " seed8=0x"
                          << static_cast<unsigned>(game::Seed8(
                                 static_cast<int>(effectiveSeed)));
            } else {
                std::cout << "unavailable seed_source=zero_sequence"
                          << " seed8=unavailable";
            }
            std::cout << std::dec << " IN_ATTACK="
                      << (((userCmd->buttons & IN_ATTACK) != 0) ? "yes" : "no")
                      << std::endl;
            std::cout << "  angles cmd=(" << userCmd->viewangles.x << ", "
                      << userCmd->viewangles.y << ")";
            Vector3 localEyeAngles{};
            if (game::GetLocalEyeAngles(local, localEyeAngles)) {
                std::cout << " fire_angle_source=(" << localEyeAngles.x << ", "
                          << localEyeAngles.y << ", " << localEyeAngles.z
                          << ")";
            } else {
                std::cout << " fire_angle_source=unavailable";
            }
            Vector3 engineAngles{};
            if (game::GetViewAngles(engineAngles)) {
                std::cout << " engine=(" << engineAngles.x << ", "
                          << engineAngles.y << ") delta=("
                          << (userCmd->viewangles.x - engineAngles.x) << ", "
                          << (userCmd->viewangles.y - engineAngles.y) << ")";
            } else {
                std::cout << " engine=unavailable";
            }
            std::cout << std::endl;

            const auto &shotTrace = GetLastShotAngleTrace();
            std::cout << "  shot pipeline: aim="
                      << (shotTrace.aimbotApplied ? "target" : "input")
                      << " recoil="
                      << (shotTrace.noRecoilRequested
                              ? (shotTrace.noRecoilApplied
                                     ? "applied"
                                     : "requested-unavailable")
                              : "off")
                      << " spread="
                      << (shotTrace.noSpreadRequested
                              ? (shotTrace.noSpreadApplied ? "applied"
                                                           : "requested-unavailable")
                              : "off")
                      << " attack=" << (shotTrace.attack ? "yes" : "no")
                      << std::endl;
            std::cout << "  shot angles: desired=(" << shotTrace.desiredAngles.x
                      << ", " << shotTrace.desiredAngles.y << ") fire_base=("
                      << shotTrace.fireBaseAngles.x << ", "
                      << shotTrace.fireBaseAngles.y << ") cmd=("
                      << shotTrace.commandAngles.x << ", "
                      << shotTrace.commandAngles.y << ") cmd_fire=(";
            const Vector3 commandFireAngles =
                shotTrace.commandAngles + shotTrace.punchAngles * 2.0f;
            std::cout << commandFireAngles.x << ", " << commandFireAngles.y
                      << ")" << std::endl;
            std::cout << "  punch current=("
                      << shotTrace.currentPunchAngles.x << ", "
                      << shotTrace.currentPunchAngles.y
                      << ") predicted_fire=";
            if (shotTrace.firePunchPredicted) {
                std::cout << "(" << shotTrace.punchAngles.x << ", "
                          << shotTrace.punchAngles.y << ") interval="
                          << shotTrace.intervalPerTick;
            } else {
                std::cout << "unavailable";
            }
            std::cout << std::endl;
            if (shotTrace.noSpreadRequested) {
                std::cout << "  predicted pellet0 sx=" << shotTrace.spreadX
                          << " sy=" << shotTrace.spreadY << " seed8=";
                if (shotTrace.seedUsable) {
                    const auto seed8 = game::Seed8(
                        static_cast<int>(shotTrace.seed));
                    std::cout << "0x" << std::hex
                              << static_cast<unsigned>(seed8)
                              << " sampler_seed=0x"
                              << (static_cast<unsigned>(seed8) + 1u)
                              << std::dec;
                } else {
                    std::cout << "unavailable";
                }
                std::cout << " residual_deg=" << shotTrace.spreadResidualDeg
                          << " iters=" << shotTrace.spreadIterations
                          << " (CS polar fire-space, first-order+iterative)"
                          << std::endl;
            }
        } else {
            std::cout << "spread diag: deferred (press F1 in CreateMove)"
                      << std::endl;
        }

        std::cout << "  perfect_nospread="
                  << (GetConfig().perfectNoSpread ? "on" : "off")
                  << " visual_norecoil="
                  << (GetConfig().visualNoRecoil ? "on" : "off")
                  << " silent_angles="
                  << (GetConfig().silentAngles ? "on" : "off")
                  << " visual_path=OverrideView_slot16_read_only"
                  << " pellet_policy=pellet0" << std::endl;
        std::cout << "  client_fire_capture="
                  << (g_clientFireDiagnostic.armed
                          ? (g_clientFireDiagnostic.commandCaptured
                                 ? "waiting_for_matching_fire_call"
                                 : "waiting_for_attack_command")
                          : "idle")
                  << std::endl;
    } else {
        std::cout << "local player: null (not in game?)" << std::endl;
    }
}

} // namespace features
