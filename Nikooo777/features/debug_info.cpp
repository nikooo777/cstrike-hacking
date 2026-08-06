#include "features/debug_info.h"

#include <iostream>
#include <string>

#include "core/constants.h"
#include "core/modules.h"
#include "features/config.h"
#include "game/entity_list.h"
#include "game/interfaces.h"
#include "game/player.h"
#include "game/weapon.h"
#include "netvars/netvars.h"
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

} // namespace

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
        for (int playerIndex = 1; playerIndex < MAXPLAYERS; ++playerIndex) {
            auto *target = game::GetPlayer(playerIndex);
            if (!game::IsValidTarget(local, target)) {
                continue;
            }

            ++validTargets;
            Vector3 headPosition{};
            if (game::GetBonePosition(target, 14, headPosition)) {
                ++readableTargetBones;
            }
        }
        std::cout << "aimbot diagnostics: enabled="
                  << (features::GetConfig().aimbot ? "yes" : "no")
                  << " validTargets=" << validTargets
                  << " readableBone14=" << readableTargetBones << std::endl;

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
            std::cout << "  mode="
                      << (spreadState.modeOk ? std::to_string(spreadState.mode)
                                             : std::string("unread"))
                      << " clip="
                      << (spreadState.clipOk ? std::to_string(spreadState.clip1)
                                             : std::string("unread"))
                      << " penalty="
                      << (spreadState.penaltyOk
                              ? std::to_string(spreadState.accuracyPenalty)
                              : std::string("unread"))
                      << std::endl;
            std::cout << "  GetInaccuracy=";
            if (spreadState.inaccuracyOk) {
                std::cout << spreadState.inaccuracy;
            } else {
                std::cout << "fail";
            }
            std::cout << " GetSpread=";
            if (spreadState.spreadOk) {
                std::cout << spreadState.spread;
            } else {
                std::cout << "fail";
            }
            std::cout << " methodsOk=" << (spreadOk ? "yes" : "no")
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

            if (seedOk && spreadState.inaccuracyOk && spreadState.spreadOk) {
                game::ConeOffsets cone{};
                // Local stream only — does not reseed process vstdlib RNG.
                if (game::PredictConeOffsets(
                        static_cast<int>(effectiveSeed), spreadState.inaccuracy,
                        spreadState.spread, cone)) {
                    std::cout << "  predicted pellet0 sx=" << cone.sx
                              << " sy=" << cone.sy << " (local RNG)"
                              << std::endl;
                    game::CompensationResult comp{};
                    if (game::CompensateAngles(userCmd->viewangles, cone.sx,
                                               cone.sy, comp) &&
                        comp.ok) {
                        std::cout << "  compensation residual_deg="
                                  << comp.forwardErrorDeg
                                  << " iters=" << comp.iterations
                                  << " (first-order+iterative)" << std::endl;
                    }
                } else {
                    std::cout << "  predicted pellet0: fail" << std::endl;
                }
            }
        } else {
            std::cout << "spread diag: deferred (press F1 in CreateMove)"
                      << std::endl;
        }

        std::cout << "  perfect_nospread="
                  << (GetConfig().perfectNoSpread ? "on" : "off")
                  << " silent_angles="
                  << (GetConfig().silentAngles ? "on" : "off")
                  << " pellet_policy=pellet0" << std::endl;
    } else {
        std::cout << "local player: null (not in game?)" << std::endl;
    }
}

} // namespace features
