#include "features/debug_info.h"

#include <cstdint>
#include <iostream>
#include <string>

#include "core/arch.h"
#include "core/constants.h"
#include "core/modules.h"
#include "features/config.h"
#include "features/bone_esp.h"
#include "features/fire_capture.h"
#include "features/perfect_nospread.h"
#include "game/entity_list.h"
#include "game/interfaces.h"
#include "game/player.h"
#include "game/weapon.h"
#include "game/weapon_math.h"
#include "netvars/netvars.h"
#include "sdk/client_offsets.h"
#include "sdk/engine_client.h"
#include "sdk/user_cmd.h"

namespace features {

namespace {

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

void PrintModuleBases() {
    std::cout << "clientModuleBase: 0x" << std::hex << core::GetModule("client.dll") << std::endl;
    std::cout << "serverModuleBase: 0x" << std::hex << core::GetModule("server.dll") << std::endl;
    std::cout << "engineModuleBase: 0x" << std::hex << core::GetModule("engine.dll") << std::endl;
}

void PrintRuntimeOffsets() {
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
    std::cout << "client-only m_iCrosshairID: 0x" << std::hex
              << sdk::offsets::kCrosshairTarget << std::dec
              << " (C_CSPlayer::GetIDTarget)" << std::endl;
    const auto clientStateAddress = game::GetClientStateAddress();
    const auto engineBase = core::GetModule("engine.dll");
    std::cout << "clientState addr: 0x" << std::hex << clientStateAddress << std::endl;
    if (clientStateAddress != 0 && engineBase != 0 && clientStateAddress >= engineBase) {
        std::cout << "clientState offset: engine.dll + 0x" << std::hex
                  << clientStateAddress - engineBase << std::dec << std::endl;
    } else {
        std::cout << "clientState offset: unavailable" << std::endl;
    }
#if ARCH_X86()
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
}

void PrintTargetDiagnostics(CCSPlayer *local) {
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
}

void PrintBoneDiagnostics(CCSPlayer *local) {
    game::BoneCacheInfo boneCache;
    const bool boneCacheUsable = game::GetBoneCacheInfo(local, boneCache);
    std::cout << "bone cache: entity=0x" << std::hex
              << boneCache.entityAddress << " matrix=0x" << boneCache.matrix
              << " count=" << std::dec << boneCache.count
              << " matrixReadable=" << (boneCache.matrixReadable ? "yes" : "no")
              << " countReadable=" << (boneCache.countReadable ? "yes" : "no")
              << " usable=" << (boneCacheUsable ? "yes" : "no")
              << " (entity + 0x" << std::hex
              << sdk::offsets::kBoneMatrix << ", count + 0x"
              << sdk::offsets::kBoneCount << ", "
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
}

void PrintWeaponSpreadState(CCSPlayer *local) {
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
}

void PrintCommandAngles(CCSPlayer *local, const CUserCmd &userCmd) {
    const auto storedSeed = userCmd.random_seed;
    std::uint32_t effectiveSeed = 0;
    bool fromStoredSeed = false;
    const bool seedOk = game::ResolveCommandRandomSeed(
        &userCmd, effectiveSeed, fromStoredSeed);
    std::cout << "  cmd.ptr=0x" << std::hex
              << reinterpret_cast<std::uintptr_t>(&userCmd)
              << " cmd.number=" << std::dec << userCmd.command_number
              << " tick=" << userCmd.tick_count
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
              << (((userCmd.buttons & IN_ATTACK) != 0) ? "yes" : "no")
              << std::endl;
    std::cout << "  angles cmd=(" << userCmd.viewangles.x << ", "
              << userCmd.viewangles.y << ")";
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
                  << (userCmd.viewangles.x - engineAngles.x) << ", "
                  << (userCmd.viewangles.y - engineAngles.y) << ")";
    } else {
        std::cout << " engine=unavailable";
    }
    std::cout << std::endl;
}

void PrintShotTrace(const ShotAngleTrace &shotTrace) {
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
}

void PrintFeatureState() {
    std::cout << "  perfect_nospread="
              << (GetConfig().perfectNoSpread ? "on" : "off")
              << " visual_norecoil="
              << (GetConfig().visualNoRecoil ? "on" : "off")
              << " silent_angles="
              << (GetConfig().silentAngles ? "on" : "off")
              << " visual_path=OverrideView_slot16_read_only"
              << " pellet_policy=pellet0" << std::endl;
    std::cout << "  client_fire_capture="
              << ClientFireCaptureStatus()
              << std::endl;
}

} // namespace

void PrintDebugInfo(const CUserCmd *userCmd, const ShotAngleTrace *shotTrace) {
    PrintModuleBases();
    PrintRuntimeOffsets();

    auto *local = game::GetLocalPlayer();
    if (local == nullptr) {
        std::cout << "local player: null (not in game?)" << std::endl;
        return;
    }

    PrintTargetDiagnostics(local);
    PrintBoneDiagnostics(local);
    std::cout << "my position:" << local->m_vecOrigin() << std::endl;

    if (userCmd != nullptr) {
        // Weapon resolution and virtual GetInaccuracy/GetSpread calls must
        // happen on the game thread. PrintDebugInfo() is also called once
        // during injection setup, where userCmd is null and the caller is
        // the loader thread; do not touch live weapon objects there.
        PrintWeaponSpreadState(local);
        PrintCommandAngles(local, *userCmd);
        if (shotTrace != nullptr) {
            PrintShotTrace(*shotTrace);
        }
    } else {
        std::cout << "spread diag: deferred (press F1 in CreateMove)"
                  << std::endl;
    }

    PrintFeatureState();
}

} // namespace features
