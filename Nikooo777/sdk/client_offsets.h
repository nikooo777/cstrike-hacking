#pragma once

#include <cstddef>
#include <cstdint>

#include "core/arch.h"

// Client-only fields and code shapes that no RecvTable or named interface
// exposes. Each group names the aidocs section holding its evidence. Re-verify
// all of them after a game update (aidocs/004 section 11.1).
namespace sdk::offsets {

#if ARCH_X64()
// aidocs/004 section 9.1: SetupBones reads [this+0xB40]/[this+0xB50] on the
// renderable subobject, which sits at entity+0x8.
inline constexpr std::size_t kBoneMatrix = 0xB48;
inline constexpr std::size_t kBoneCount = 0xB58;

// aidocs/004 section 9.3: C_CSPlayer::GetIDTarget.
inline constexpr std::size_t kCrosshairTarget = 0x1B20;

// aidocs/005 section 5.6: movement type read by the penalty decay.
inline constexpr std::size_t kMoveType = 0x1F4;

// aidocs/005 sections 5.3 to 5.6: C_WeaponCSBase fields.
inline constexpr std::size_t kWeaponAccuracyState = 0xCA8;
inline constexpr std::size_t kWeaponInfoIndex = 0xC62;
inline constexpr std::size_t kWeaponAccuracyModifier = 0xBF4;
#else
// aidocs/002: legacy x86 client layout.
inline constexpr std::size_t kBoneMatrix = 0x578;
inline constexpr std::size_t kBoneCount = 0x57C;
inline constexpr std::size_t kCrosshairTarget = 0x14F0;
inline constexpr std::size_t kDormant = 0x17E;
#endif

// aidocs/005 sections 3.3 and 5.6: CCSWeaponInfo fields, relative to the
// object returned by the weapon-info lookup.
namespace weapon_info {
#if ARCH_X64()
inline constexpr std::size_t kAccuracyQuadratic = 0x8CC;
inline constexpr std::size_t kAccuracyDivisor = 0x8D0;
inline constexpr std::size_t kAccuracyOffset = 0x8D4;
inline constexpr std::size_t kMaxInaccuracy = 0x8D8;
inline constexpr std::size_t kCrouchInaccuracy = 0x8E4;
inline constexpr std::size_t kStandInaccuracy = 0x8EC;
inline constexpr std::size_t kLadderInaccuracy = 0x904;
inline constexpr std::size_t kStandRecovery = 0x91C;
inline constexpr std::size_t kCrouchRecovery = 0x920;
inline constexpr std::size_t kAccuracyModifierBaseline = 0x924;
#else
inline constexpr std::size_t kAccuracyQuadratic = 0x89C;
inline constexpr std::size_t kAccuracyDivisor = 0x8A0;
inline constexpr std::size_t kAccuracyOffset = 0x8A4;
inline constexpr std::size_t kMaxInaccuracy = 0x8A8;
#endif
} // namespace weapon_info

#if ARCH_X64()
// aidocs/005 section 5.3: GetInaccuracy loads the weapon_accuracy_model parent
// ConVar with MOV RAX,[RIP+rel32] at +0x06 and compares m_nValue with 1 at
// +0x10.
namespace accuracy_model {
inline constexpr std::size_t kLoadOffset = 0x6;
inline constexpr std::uint8_t kLoad[] = {0x48, 0x8B, 0x05};
inline constexpr std::size_t kLoadDisplacement = 0x3;
inline constexpr std::size_t kLoadLength = 0x7;
inline constexpr std::size_t kCompareOffset = 0x10;
inline constexpr std::uint8_t kCompare[] = {0x83, 0x78, 0x58, 0x01};
inline constexpr std::size_t kConVarIntValue = 0x58;
} // namespace accuracy_model
#endif

} // namespace sdk::offsets
