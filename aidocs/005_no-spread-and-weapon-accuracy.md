# 005 - No-spread and weapon accuracy

This chapter reverses where a Counter-Strike: Source bullet actually goes. It
follows the command seed, the weapon's accuracy state, the fire-time cone, and
the recoil punch, and shows how to compensate for them in `CUserCmd` without
shaking the local camera.

The emphasis is on **which subsystem owns each value**: what is networked and
what is client-only, when each value changes between `CreateMove` and the
shot, and what a diagnostic should print when a stage cannot be validated.
Matching the local client path does not prove that a remote server accepts a
compensated shot.

Use this only against binaries and processes you are permitted to study,
preferably offline or local. Client-side angle tricks that "look perfect" can
still disagree with a server that recomputes hits independently.

**Status:** validated for one x64 build with matching live captures (section
12.5). `perfect_nospread` still defaults to `false` in both profiles. The x86
cone path, the `weapon_accuracy_model 1` branch, and remote-server behavior are
not validated (section 16).

## Binaries used for this chapter

| Item | Value |
| --- | --- |
| Client | `cstrike/bin/x64/client.dll`, Ghidra `/source-engine-tutorial/x64/client.dll` |
| Server comparison | `cstrike/bin/x64/server.dll`, Ghidra `/source-engine-tutorial/x64/server.dll.1` |
| x86 cross-check | Ghidra `/client.dll` (`x86:LE:32:default`, image base `0x10000000`) |
| x64 language / image base | `x86:LE:64:default` / `0x180000000` |

The similarly named `server.dll` Ghidra import is malformed. The on-disk server
binary was checked separately to confirm the same CS polar sequence.

Module-relative addresses describe the August 2026 x64 build. The 2026-09-20
update moved this chapter's `client.dll` code by about `-0xE10`. Every
chapter-005 signature still matches uniquely, every weapon and movement slot
used here is unchanged, and chapter 004 section 11.1 maps each function cited
below to its 2026-09-20 address.

Cross-check sources: Source SDK 2013 (`in_main.cpp` seed assignment,
`MD5_PseudoRandom`, `iconvar.h` flags, `convar.h` layout), RecvTable string
anchors in this client, and the `CreateMove` hook from
[004](004_x64-migration-and-abi.md).

## Terms used in this chapter

These names appear in the code, the F1 output, and the sections below.

| Term | Meaning |
| --- | --- |
| `command_number` | Sequence number of a `CUserCmd`. Zero on the temporary extra-mouse-sample command. |
| `random_seed` | `MD5_PseudoRandom(command_number) & 0x7fffffff`. The engine writes it *after* `ClientMode::CreateMove`. |
| `seed8` | `random_seed & 0xFF`, the only part the weapon fire path uses. |
| inaccuracy | Radius returned by `GetInaccuracy()` (vtable slot 382). Sampled once per shot. |
| spread | Radius returned by `GetSpread()` (slot 383). Sampled once per pellet. |
| penalty | `m_fAccuracyPenalty` at `weapon+0xCB0`. Decays every tick and rises after each shot. |
| accuracy model | Value of the replicated ConVar `weapon_accuracy_model` (default `2`). Value `1` selects a separate path that is not modeled. |
| punch | `m_vecPunchAngle`, the recoil kick at `player+0x127C`. |
| desired | The angle the player or the aimbot wants the bullet to take. |
| fire space | Angles as the fire path consumes them: the command angle plus `2*punch`. |
| `fire_base` | The desired direction in fire space: `desired + 2*punch` with no-recoil off, `desired` with no-recoil on. |
| `cmd` / `cmd_fire` | The final `CUserCmd::viewangles`, and that angle plus `2*predicted punch`. |
| `(sx, sy)` | Cone offsets in the right/up plane of the fire angles. |
| `residual_deg` | Angle between the simulated post-spread direction of the compensated command and `fire_base`. |
| `pellet0` | Multi-pellet policy: compensate for pellet 0 only (section 6.4). |
| silent | Returning `false` from `CreateMove` so its caller does not copy `cmd` into the camera (section 9). |

## 1. What "no-spread" means

Do not start by searching for an old "no-spread offset" from a public list.
The bullet direction comes out of a pipeline, and "no-spread" can mean any of
three different interventions:

1. **Visual-only:** zero the cone inside `FX_FireBullets` or force
   `GetInaccuracy`/`GetSpread` to `0` on the local client.
2. **Angle compensation ("perfect no-spread"):** leave the CS fire path alone,
   replay its deterministic seed and cone samples, and pre-adjust
   `CUserCmd::viewangles` so that the *post-spread* direction matches the
   intended aim.
3. **State wipe:** zero `m_fAccuracyPenalty` or related weapon fields. This is
   partial: movement and stand/crouch tables still contribute.

**Target for this tutorial:** (2), plus a **silent** camera layer so the local
view does not show the compensated angles jittering on every shot. The crude
approaches (1) and (3) are documented only as a contrast. Each has different
observability and a different interaction with the server.

## 2. The validated pipeline at a glance

```text
ClientMode::CreateMove (our hook runs here)
  cmd.command_number is set; cmd.random_seed is still 0
  -> caller writes random_seed = MD5_PseudoRandom(command_number) & 0x7fffffff
  -> prediction publishes that seed to a client global
  -> movement applies one CSS punch-decay tick                     (section 7.1)
  -> weapon frame applies one m_fAccuracyPenalty decay tick         (section 5.6)
  -> weapon wrapper reads GetInaccuracy() and GetSpread()           (section 5.4)
  -> FX_FireBullets(seed8, inaccuracy, spread):
       RandomSeed(seed8 + 1)
       one polar sample at the inaccuracy radius
       one polar sample per pellet at the spread radius             (section 6.2)
  -> FireBullet: dir = forward + right*sx + up*sy on (cmd + 2*punch) (section 6.3)
  -> weapon wrapper adds the post-shot penalty                      (section 5.7)
```

Everything after our hook is deterministic given the command number and the
weapon state. The hook therefore predicts each later stage instead of reading
it:

```text
CreateMove hook:
  desired aim (input or aimbot)
    -> predicted fire-time punch and fire-time inaccuracy
    -> fire_base in fire space (depends on the no-recoil toggle)
    -> replay the cone for seed8 + 1 and invert it around fire_base
    -> cmd = compensated fire angle - 2*predicted punch
    -> return false when the camera must not adopt cmd              (section 9)

OverrideView hook:
  call original -> subtract one punch from CViewSetup::angles       (section 9.4)
```

### 2.1 Who owns each stage

| Stage | Where it lives | Authoritative for multiplayer hits? |
| --- | --- | --- |
| `CUserCmd::viewangles` / buttons | Client to server | Server simulates the command |
| `random_seed` | Client fills it; the server derives it the same way from the sequence | Shared **if** both sides use the same seed rule |
| `GetInaccuracy` / `GetSpread` | Client, mirrored by server game code | Inputs to the CS fire path |
| CS `FX_FireBullets` polar cone | Client and server CS fire implementation | **Authority-shaped direction** when seed and radii state match |
| Generic `CShotManipulator` / `m_vecSpread` helper | A separate shared `FireBullets` path | Not the CS predictor target |
| `CTEFireBullets` | Network temp entity | Other clients' visuals only |

A hook that only patches the client-side `FX_FireBullets` can produce
perfect-looking tracers while a server with different state spreads the real
shot. Command compensation targets the CS polar math, but it still depends on
matching seed timing, getter values, vector mapping, lag compensation, and
server code. Record observations honestly: "tracers straightened" is not
"server accepted perfect aim".

### 2.2 Where each piece lives

| File | Responsibility |
| --- | --- |
| `hooks/create_move.cpp` | Zero-sequence guard, feature order, the camera return rule |
| `features/perfect_nospread.cpp` | Reads live state, builds the request, returns the shot trace that `CreateMove` passes to F1 and the capture |
| `features/norecoil.cpp` | Checked punch read through the `DT_Local` netvars |
| `hooks/override_view.cpp` | Read-only visual punch removal and the view snapshot for chapter 006 |
| `game/weapon.cpp` | Active weapon, getter calls, accuracy-model read, weapon-info lookup, pre-fire decay reads |
| `sdk/client_offsets.h` | The client-only weapon, weapon-info, and prologue-shape offsets this chapter uses |
| `game/weapon_math.cpp` | Command seed, local RNG, cone replay, inverse cone, `ComposeShotAngles`, punch decay, penalty-decay rule |
| `game/timing.cpp` | Tick interval from `CGlobalVarsBase+0x1C` through the `GlobalVars` signature |
| `hooks/client_fire_bullets.cpp`, `hooks/update_accuracy_penalty.cpp` | One-shot x64 diagnostic detours |
| `features/fire_capture.cpp` | The x64 fire-time capture those detours feed (section 11.2) |
| `features/debug_info.cpp` | F1 output (section 11.1) |
| `tests/weapon_math_tests.cpp` | Offline seed, layout, cone, and composition tests |

Resolution stays in `game/` and `game/interfaces.cpp`, not in feature bodies.
Do not scan signatures or walk weapons inside `EndScene`.

### 2.3 Suggested order for reproducing this chapter

1. Read-only weapon, seed, and separate-radii diagnostic (F1).
2. Prove the CS polar reconstruction offline, with deterministic vectors and no
   mutation.
3. Command-only compensation behind a default-off toggle. Expect camera shake.
4. Return `false` for changed command angles (section 9). The shake goes away.
5. Confirm that visual no-recoil still composes. Only then consider local FX
   cosmetics.

## 3. Semantic anchors in the client

### 3.1 Strings

| String | Address | Role |
| --- | ---: | --- |
| `DT_TEFireBullets` | `client.dll+0x475068` | Temp-entity table for bullet FX |
| `m_fInaccuracy` / `m_fSpread` / `m_iSeed` | near `+0x4750D0` | Fields on that temp entity |
| `FX_FireBullets: weapon alias...` | `+0x47D698` | Error path inside `FX_FireBullets` |
| `Spread`, `InaccuracyStand`, ... | near `+0x47D328` | Weapon script keys |
| `weapon_accuracy_logging` | `+0x47D680` | ConVar registration |
| `weapon_accuracy_model` | `+0x4AC628` | ConVar registration (section 5.3) |
| `m_fAccuracyPenalty` | `+0x4ADB28` | `DT_WeaponCSBase` netvar |
| `m_weaponMode` | `+0x4ADB18` | `DT_WeaponCSBase` netvar |
| `m_hActiveWeapon` | `+0x400FE8` | `DT_BaseCombatCharacter` |
| `m_iClip1` | `+0x3F7AD8` | `DT_LocalWeaponData` |
| RTTI `.?AVC_WeaponCSBase@@` | `+0x5DB3E8` | Weapon class identity |

### 3.2 Netvar offsets from RecvProp registration

| Table | Property | Offset |
| --- | --- | ---: |
| `DT_WeaponCSBase` | `m_weaponMode` | `0xCAC` |
| `DT_WeaponCSBase` | `m_fAccuracyPenalty` | `0xCB0` |
| `DT_BaseCombatCharacter` | `m_hActiveWeapon` | `0x11A0` |
| `DT_CSLocalPlayerExclusive` | `m_iShotsFired` | `0x1A34` |
| `DT_LocalWeaponData` | `m_iClip1` | `0xC30` |
| `DT_TEFireBullets` | `m_iSeed` / `m_fInaccuracy` / `m_fSpread` | `0x44` / `0x48` / `0x4C` |

The implementation reads these through `netvars::GetOffset` at runtime. The
table is a static cross-check.

The temp-entity displacements are relative to the **networked data base** that
`FUN_18001DF00` registers. `CTEFireBullets::PostDataUpdate` (`FUN_1801F0500`)
uses a `this` that is eight bytes earlier (the vtable / `C_BaseTempEntity`
header), so its listing shows different numbers for the same fields:

| Field | RecvProp (data base) | PostDataUpdate `this` |
| --- | ---: | ---: |
| `m_vecOrigin` | `+0x24` | `+0x1C` |
| `m_iSeed` | `+0x44` | `+0x3C` |
| `m_fInaccuracy` | `+0x48` | `+0x40` |
| `m_fSpread` | `+0x4C` | `+0x44` |

Both columns are correct for their pointer basis. When reading listing
displacements, always record **which `this`** they are relative to.

### 3.3 Weapon script table (`CCSWeaponInfo`)

`FUN_1801FDC30` (`client.dll+0x1FDC30`) parses script keys into a weapon-info
object. These displacements are on that object, not on the weapon entity:

| Key | Offset |
| --- | ---: |
| `MaxPlayerSpeed` | `+0x728` |
| `Bullets` | `+0x8C4` |
| `AccuracyQuadratic` | `+0x8CC` |
| `AccuracyDivisor` | `+0x8D0` |
| `AccuracyOffset` | `+0x8D4` |
| `MaxInaccuracy` | `+0x8D8` |
| `Spread` / `SpreadAlt` | `+0x8DC` / `+0x8E0` |
| `InaccuracyCrouch` / `InaccuracyStand` / `InaccuracyJump` / ... | `+0x8E4` / `+0x8EC` / ... |
| `InaccuracyFire` (+ alt) | `+0x90C` / `+0x910` |
| `InaccuracyMove` (+ alt) | `+0x914` / `+0x918` |

Many of these are paired primary/alternate floats. Primary fire indexes them
with `m_weaponMode` (`0` or `1`) as `offset + 4*mode`.

## 4. The command seed

### 4.1 `CUserCmd` x64 layout

The `sdk/user_cmd.h` declaration matches the live prediction code once the
vtable pointer is accounted for:

| Field | Offset |
| --- | ---: |
| vptr | `+0x00` |
| `command_number` | `+0x08` |
| `tick_count` | `+0x0C` |
| `viewangles` | `+0x10` |
| `forwardmove` / `sidemove` / `upmove` | `+0x1C` / `+0x20` / `+0x24` |
| `buttons` | `+0x28` |
| `impulse` | `+0x2C` (+3 pad) |
| `weaponselect` / `weaponsubtype` | `+0x30` / `+0x34` |
| **`random_seed`** | **`+0x38`** |
| `mousedx` / `mousedy` | `+0x3C` / `+0x3E` |
| `hasbeenpredicted` | `+0x40` |

The offline `weapon_math_tests` target checks the field offsets relative to
`command_number` (for example `random_seed == command_number + 0x30`), so an
x86/x64 packing drift fails a test instead of silently reading the wrong field.
Do not assume an x86 overlay without re-measuring.

Evidence: the prediction helper `FUN_1800524C0` stores `*(cmd + 0x38)` into the
global `DAT_1805A6000` (`client.dll+0x5A6000`). `FUN_180187440`
(RunCommand-style) calls that helper at command start and clears the seed again
at the end.

### 4.2 When the seed is assigned

Source SDK 2013 `in_main.cpp` assigns:

```cpp
cmd->random_seed = MD5_PseudoRandom( sequence_number ) & 0x7fffffff;
```

The seed is **deterministic from the command sequence**, not free entropy.
That is what makes angle compensation possible in principle on a matching
client/server RNG path.

Ghidra confirms the order in the x64 client. The normal command path
`FUN_180152290` writes `command_number = sequence_number` at `cmd + 0x08`,
calls `ClientMode::CreateMove`, and only then writes `random_seed` at
`cmd + 0x38`. At our hook the field is normally still zero, so
`game::ResolveCommandRandomSeed` derives the seed from `command_number` with a
local one-block `MD5_PseudoRandom` implementation. It uses a stored seed only
when the field is already nonzero.

### 4.3 Zero-sequence extra samples

There is a second input path. `FUN_1801527B0` calls the same
`ClientMode::CreateMove` slot with a temporary stack `CUserCmd` whose
`command_number` stays zero. That is an extra mouse sample, not a firing
command, and `MD5_PseudoRandom(0)` is not a real seed for it.

The hook therefore returns immediately for `command_number <= 0`: no buttons,
aim, recoil, or spread mutation. If the F1 key edge lands on such a sample, the
dump is deferred to the next real command. Otherwise every extra sample could
hide a seed or ordering bug. A valid runtime dump shows positive, changing
command numbers while firing, `seed_source=command_number`, and a changing
`seed8`. `effective_seed=unavailable seed_source=zero_sequence` identifies a
sample that must not be used.

### 4.4 Seed width: weapon path versus temp-entity path

Every local weapon fire caller of `FX_FireBullets` on the x64 sample
(`FUN_180236AB0`, `FUN_1802373A0`, ...) masks the published seed to its low
byte:

```asm
MOV   EBX, dword ptr [client.dll+0x5A6000]  ; cmd.random_seed, published by prediction
MOVZX ESI, BL                               ; seed8 = random_seed & 0xFF
; ... ESI is passed to FX_FireBullets as the seed argument
```

x86 fire (`FUN_1020B490`) does the same with `MOVZX EAX, BL` before
`CALL FX_FireBullets`. The fire function then adds one before seeding its
stream: `RandomSeed(seed8 + 1)`. Compensation must use the same mask and
increment, not the full 31-bit value.

The temp-entity replay is different:

```text
FUN_1801F0500 (CTEFireBullets PostDataUpdate):
  FX_FireBullets(..., *(this+0x3C), *(this+0x40), *(this+0x44), ...)
  ; this+0x3C is m_iSeed on the temp-entity object basis (RecvProp +0x44)
  ; no MOVZX: the full int is passed
```

| Path | Seed argument | Role for compensation |
| --- | --- | --- |
| CS client/server fire path | `random_seed & 0xFF`, then `+1` internally | **Compensation target** |
| Local weapon wrapper | passes `random_seed & 0xFF` to the fire path | Supplies the command seed |
| `CTEFireBullets` replay | networked `m_iSeed` (full int) | Other clients' **FX only** |

Do not assume the temp-entity `m_iSeed` equals the full `cmd.random_seed`. If a
test needs remote tracers to match, dump both values on the same fire tick.

## 5. Weapon state and the fire-time radii

### 5.1 Active weapon handle to pointer

`m_hActiveWeapon` lives at `player+0x11A0`. The client's own resolver
`FUN_18004A890` (`client.dll+0x4A890`) is the reference behavior:

```text
handle = *(uint32*)(player + 0x11A0)
if handle == 0: return null

index  = (handle == 0xFFFFFFFF) ? 0x1FFF : (handle & 0xFFFF)
entry  = entity_list_base + index * 0x20 + 8
if entry.serial != (handle >> 16): return null
return entry.pointer
```

The low 16 bits are the list index and the high 16 bits are the serial. The
`0x20` stride is the internal list record, not the public `IClientEntityList`
layout, and `entity_list_base` (`PTR_DAT_1805AE9F8` in this sample) is a
data-flow observation, not an address to hardcode.

The implementation does not re-implement that walk. It passes the handle to
`VClientEntityList003::GetClientEntityFromHandle` (slot 4). That parameter is a
`CBaseHandle` **by value**, which MSVC x64 passes as a pointer to a temporary.
Declaring it as `uint32_t` reads the handle correctly and then returns null for
every lookup. Chapter 004 section 7.1 has the listing.

### 5.2 Weapon virtual methods

| Method | Slot | x64 vtable byte offset | Sample implementation |
| --- | ---: | ---: | --- |
| weapon id (for the fire call) | 371 | `+0xB98` | |
| `GetInaccuracy` | 382 | `+0xBF0` | `FUN_180232900`; the default model continues in `FUN_180235B10` |
| `GetSpread` | 383 | `+0xBF8` | `FUN_180235D90` |
| `UpdateAccuracyPenalty` | 384 | `+0xC00` | `client.dll+0x2367D0` for the live AK |

The weapon-speed method at byte offset `+0xB90` is also used (section 5.4).
These slots belong to **this** build's `C_WeaponCSBase` layout. Re-verify them
if the vtable grows. The x86 build has the same slot indices at different byte
offsets (section 15.2).

The getters are called through the live weapon's vtable. Each call is wrapped
in an exception guard, and the result must be finite and inside `[0, 5]`.

### 5.3 The accuracy-model selector is a replicated ConVar

`GetInaccuracy` starts by testing a selector and takes a separate path when it
equals `1`. The selector is the `weapon_accuracy_model` ConVar.

`game::ReadAccuracyModel` reads it exactly the way the getter does. It checks
the getter's prologue bytes, `MOV RAX,[RIP+rel32]` at `+0x06` and
`CMP dword ptr [RAX+0x58],1` at `+0x10`, decodes the RIP-relative slot, reads
the parent ConVar pointer, and reads `m_nValue` at `+0x58`. If the prologue
does not match, the model is unavailable and the pre-fire prediction fails
closed. F1 prints the result as `accuracy_model=` and on the
`accuracy model:` line (section 11).

The 2026-09-20 client confirms the identity (addresses from that build). A byte
search for the prologue shape finds 23 functions that test the selector the
same way, as expected when the CS weapon classes override the accuracy
methods. The function at `0x180231AD0` is the `C_AK47` vtable's slot 382 in
that build (August: `0x180232900`):

```asm
PUSH RBX
SUB  RSP, 0x20
MOV  RAX, qword ptr [0x1806991A8]
MOV  RBX, RCX
CMP  dword ptr [RAX + 0x58], 0x1
JNZ  ...
```

A static initializer builds the ConVar object that the slot belongs to:

```asm
MOV  R9D, 0x2082                 ; flags
LEA  R8,  [0x1804032F0]          ; default value "2"
LEA  RDX, [0x1804AC170]          ; "weapon_accuracy_model"
LEA  RCX, [0x180699170]          ; the ConVar object
CALL 0x1802F8790                 ; ConVar constructor
```

The slot `0x1806991A8` is `ConVar + 0x38`. Laying out the Source SDK 2013
`ConVar` class for x64 puts `m_pParent` at `+0x38` and `m_nValue` at `+0x58`,
so the getter reads `weapon_accuracy_model.GetInt()` through the parent
pointer. Decoded against Source SDK 2013 `iconvar.h`,
the flags are `FCVAR_REPLICATED | FCVAR_ARCHIVE | FCVAR_DEVELOPMENTONLY`.

Practical consequences:

- the value comes from the server, and a server that sets
  `weapon_accuracy_model 1` switches every weapon to the unmodeled path;
- the default `2` takes the path modeled in this chapter;
- `GetSpread` returns `0` when the model is `1`, so both getters change
  together.

Model `1` uses `weapon+0xCA8` with player-state-dependent coefficients. The
current compensation does not model it and fails closed when it is observed.

### 5.4 What the getters compute (default model)

`GetSpread` returns `Spread[mode]` from weapon info (`+0x8DC + 4*mode`).

`GetInaccuracy` combines a movement term with the current penalty. Ghidra's
tail at `client.dll+0x235B10` calls the absolute-velocity updater at
`+0x85700`, reads horizontal velocity from `player+0x1A8/+0x1AC`, and computes:

```text
speed    = length2D(player absolute velocity)
lower    = weapon_speed * 0.34
upper    = weapon_speed * 0.95
movement = clamp((speed - lower) / (upper - lower), 0, 1)
           * weapon_info[0x914 + mode*4]           ; InaccuracyMove
GetInaccuracy = movement + weapon[0xCB0]           ; + m_fAccuracyPenalty
```

The constants are the floats at `client.dll+0x47CB54` (`0.34`) and
`client.dll+0x418F1C` (`0.95`). `weapon_speed` comes from the method at vtable
byte offset `+0xB90`; zero falls back to weapon-info `+0x728`.

The important consequence: `CreateMove` runs one tick before the fire call,
and the penalty decays in between (section 5.6), so the getter value read in
the hook is not the value the shot will use. The movement term was measured to
match at fire time (section 12.3), so only the penalty needs predicting:

```text
fire_inaccuracy = GetInaccuracy() - penalty + decayed_penalty
```

### 5.5 Weapon-info lookup: use `weapon+0xC62`

The table lookup is `FUN_1801DC630` (`client.dll+0x1DC630`). Ghidra shows a
`ushort` argument, a `0x18`-byte table stride, and the selected weapon-info
pointer at table entry `+0x10`. Invalid ids return a fallback object. The x86
analogue is `FUN_101C0C50` (`client.dll+0x1C0C50`) with a `0x10`-byte stride.

The index the accuracy code passes is **not** the virtual weapon id (slot 371).
Ghidra shows the accuracy helper at `client.dll+0x4D700` reading a `ushort`
from `weapon+0xC62` and tail-jumping to the lookup. Slot 371 is the separate id
passed to the fire call. For the captured AK the fire id was 27 and the
accuracy-info index was 46. Section 12.4 describes how using slot 371 produced
a small but consistent error.

The configured `WeaponInfoLookup` pattern resolves the function entry itself
(`operand=match`). It is optional, so a changed build fails closed instead of
calling an unverified helper. The wildcarded searches are unique in both
sample clients.

### 5.6 Pre-fire penalty decay (slot 384)

`client.dll+0x235FC0` is the weapon frame path. Its first virtual call is slot
384 (`+0xC00`), and the attack checks happen afterward. For the live AK, slot
384 points to `client.dll+0x2367D0`. That function reads `weapon+0xCB0`,
selects a baseline and a recovery time from the movement state, and performs:

```text
factor       = exp(decay_constant / recovery_time * interval_per_tick)
next_penalty = factor * (current_penalty - baseline) + baseline
```

If the current penalty is below the selected baseline, it is raised directly
to the baseline. `FUN_1803AFD60`, called with the scaled value in `XMM0`, is the
binary's float exponential.

| Input | Source |
| --- | --- |
| move type | `player+0x1F4`; value `9` selects the ladder path |
| flags | `m_fFlags` (`player+0x440` in the listing); `FL_ONGROUND` and `FL_DUCKING` |
| mode | `weapon+0xCAC`, indexes the paired weapon-info floats |
| current penalty | `weapon+0xCB0` |
| extra baseline | byte at `weapon+0xBF4`; when set, add weapon-info `+0x924` |
| tick interval | `CGlobalVarsBase+0x1C`, through the `GlobalVars` signature |

| State | Baseline (`+ mode*4`) | Recovery time | Decay constant |
| --- | --- | ---: | ---: |
| ladder | `+0x8EC` (stand) plus `+0x904` (ladder) | `+0x91C` | `-2.3025851` |
| ducking, on ground | `+0x8E4` (crouch) | `+0x920` | `-2.3025851` |
| standing, on ground | `+0x8EC` (stand) | `+0x91C` | `-2.3025851` |
| airborne, standing | `+0x8EC` (stand) | `+0x920` | `-0.7675284` |
| airborne, ducking | `+0x8E4` (crouch) | `+0x920` | `-0.7675284` |

`game::SelectPenaltyDecayRule` encodes this table as pure logic, and the
offline tests check every row; `game/weapon.cpp` only reads the fields the rule
names. The `GlobalVars` signature anchors this
exact sequence, and its RIP-relative `MOV RAX,[slot]` loads the pointer slot;
one pointer read yields `CGlobalVarsBase`. The signature is unique in both
builds:

| Build | Match | Decoded slot |
| --- | ---: | ---: |
| August | `client.dll+0x2368E4` | `client.dll+0x5AE280` |
| 2026-09-20 | `client.dll+0x235AD4` | `client.dll+0x5AE280` |

The slot lies inside `.data` and is referenced from about 1,175 places in
`.text` in both builds, as expected for `gpGlobals`. The August value comes from
decoding the recovered August binary (chapter 004, section 11.1).

### 5.7 Post-shot penalty

After `FX_FireBullets` returns, the weapon wrapper adds the per-shot penalty
(section 6.1). It affects the *next* shot, never the current one. The F1 output
keeps it as a separate `nextPenalty` value so it is not confused with the
pre-fire decay.

The accuracy scalar at `weapon+0xCA8` is also updated before the getters, but
the default model does not consume it.

## 6. The CS fire path

### 6.1 The client weapon wrapper

`FUN_180236AB0` (`client.dll+0x236AB0`) is the client weapon/FX wrapper. It
updates local accuracy state and then feeds the fire path. It is not the shared
hit-trace function. Listing evidence, with the weapon in `R15` and the owner
player in `RBP`:

```text
shots = ++player->m_iShotsFired
accuracy_state = shots^2 (or shots^3) / AccuracyDivisor + AccuracyOffset
weapon+0xCA8 = min(accuracy_state, MaxInaccuracy)

CALL   qword ptr [RAX + 0xBF8]   ; GetSpread, float in XMM0
CALL   qword ptr [RAX + 0xBF0]   ; GetInaccuracy, float in XMM0

MOV    EBX, dword ptr [client.dll+0x5A6000]  ; prediction seed global
MOVZX  ESI, BL

CALL   FX_FireBullets(... seed=ESI, inaccuracy, spread ...)

; only after FX_FireBullets returns:
weapon+0xCB0 += weapon_info[mode].accuracy_penalty_per_shot
```

The byte at `AccuracyQuadratic` selects square (nonzero) versus cubic (zero)
shot scaling.

| Weapon field | Offset | Notes |
| --- | ---: | --- |
| `m_iClip1` | `0xC30` | decremented on fire |
| `m_weaponMode` | `0xCAC` | alternate-fire table index |
| accuracy scalar | `0xCA8` | derived client value, unused by the default model |
| `m_fAccuracyPenalty` | `0xCB0` | netvar |

The shots-fired counter is `m_iShotsFired` at `player+0x1A34`, registered on
`DT_CSLocalPlayerExclusive` in `FUN_18001D2E0`. The implementation reads it
through the netvar name.

### 6.2 `FX_FireBullets` cone math

| Binary | Function | Module-relative address | Evidence |
| --- | --- | ---: | --- |
| x64 client | `FX_FireBullets` (`FUN_1801FFA30`) | `client.dll+0x1FFA30` | `RandomSeed(seed+1)`, `2π` angle/radius draws, separate inaccuracy/spread inputs |
| x64 client | weapon wrapper | `client.dll+0x236AB0` | calls slots 382/383, passes the low-byte seed and both getter results |
| x64 server | CS `FX_FireBullets` | `server.dll+0x3252E0` | same `seed+1`, separate radii, polar pair helper, per-pellet loop |
| x64 client | sin/cos pair helper (`FUN_18038F140`) | `client.dll+0x38F140` | builds the two-float pair used by the cone code |
| x64 server | sin/cos pair helper | `server.dll+0x41CD70` | same role in the server copy |

The imports are vstdlib's `RandomSeed` and `RandomFloat` (IAT
`client.dll+0x3E15D8` / `+0x3E15E0`). The constant at `client.dll+0x3FB6E8` is
`0x40C90FDB`, which is `2π`. The listing shape when a local shooter exists:

```text
; weapon path: seed_argument is already (random_seed & 0xFF)
RandomSeed(seed_argument + 1)

; shared inaccuracy sample, once per shot
θ0 = RandomFloat(0, 2π)
r0 = RandomFloat(0, inaccuracy)
(x0, y0) = (cos(θ0) * r0, sin(θ0) * r0)

; per pellet (CCSWeaponInfo Bullets, x64 +0x8C4)
for pellet in 0 .. bullets-1:
    θ = RandomFloat(0, 2π)
    r = RandomFloat(0, spread)
    (sx, sy) = (x0 + cos(θ) * r, y0 + sin(θ) * r)
    FireBullet(origin, angles, ..., sx, sy)
```

Lane order is easy to get wrong. The helper computes `sin(θ)` and
`sin(θ + π/2)` (that is, `cos(θ)`) and returns them as
`(low, high) = (sin θ, cos θ)`. The x64 fire call consumes the high lane as
right and the low lane as up, so `(right, up) = (cos θ, sin θ)`. Verify both
the helper's return order and the consumer's shuffle after a binary update.

The server function is the stronger authority-side evidence. It begins with
the seed increment, loads the `2π` constant, calls the angle and radius imports,
calls the pair helper, and repeats the spread pair inside the pellet loop.

### 6.3 `FireBullet` direction construction

`FUN_1801F9BB0` applies the offsets after `AngleVectors` on the fire angles
(`FUN_1802AEA60`, degrees to radians with `* 0.017453292`):

```text
AngleVectors(angles, forward, right, up)
dir = normalize(forward + right * sx + up * sy)
; then trace along dir
```

The fire angles are the command angles plus `2*punch` (section 7.2). The
channel that compensation must invert is therefore:

```text
post_spread_dir = normalize(F + R*sx + U*sy)   with F, R, U from (cmd + 2*punch)
```

with `(sx, sy)` produced by the polar sequence for the same seed and radii the
weapon will use on that tick.

### 6.4 Multi-pellet policy

`Bullets` (x64 weapon-info `+0x8C4`) is the pellet count. Shotguns use values
greater than 1, while rifles and pistols use 1. Each shot takes one inaccuracy
sample and one spread sample *per pellet*. One `CUserCmd` has one aim
direction, so only one pellet can be placed exactly.

| Policy | Behavior | When |
| --- | --- | --- |
| **`pellet0`** (implemented, fixed) | Compensate using pellet 0's offsets | Always; exact for single-bullet weapons |
| `none` | Skip compensation if `Bullets != 1` | Possible strict mode, not implemented |
| `center` | Compensate using the average of all pellet offsets | Possible experiment, not "perfect" |

F1 prints `pellet_policy=pellet0`. Never claim "perfect for all pellets" when
`Bullets > 1`.

### 6.5 Replaying the RNG locally

Game fire uses the process-global vstdlib `RandomSeed` / `RandomFloat` stream.
The prediction uses a **local** `CUniformRandomStream`-style generator and
never reseeds the shared stream from the hook. Reseeding the game's stream
would change the shot it is trying to predict.

Ghidra inspection of this build's `vstdlib.dll` exports verified the same seed
normalization, Park–Miller step, 32-entry shuffle table, float scale, and upper
clamp as the local replay. Proof status:

1. Two local predictions with the same inputs match bit-for-bit in the offline
   tests.
2. The active `vstdlib.dll` implementation matches the local generator.
3. The one-shot x64 `FX_FireBullets` detour, armed by F1, reports the actual
   seed, radii, and fire angles beside the `CreateMove` prediction
   (section 11.2).

### 6.6 Paths that are not the compensation target

- **Generic `FireBullets`:** `client.dll+0x4FF30` and `server.dll+0x137890` are
  shared helpers with a `CShotManipulator` / `m_vecSpread` shape. They are a
  different path and must not replace the CS weapon path because their names
  look related (section 12.1).
- **Temp-entity replay:** `FUN_1801F0500` passes the full networked seed and
  uses a different object basis. It is useful for validating remote effects,
  not as a replacement for the command predictor.

## 7. Punch at fire time

### 7.1 CSS punch decay (movement slot 15)

Punch decays once between `CreateMove` and the fire call. The
`CCSGameMovement` RTTI locator is at `client.dll+0x54B090`, and its type
descriptor resolves to `.?AVCCSGameMovement@@` at `+0x5E34C8`. The locator
precedes the vtable, which starts at `client.dll+0x4781C8`. Slot 15 (`+0x78`,
stored at `client.dll+0x478240`) points to `client.dll+0x1F5AE0`.

That is the live CSS punch decay, not the base damped-spring implementation at
`client.dll+0x119390`. It reads the player from movement-object `+0xEE0`, then
uses `player+0x127C` and the same `CGlobalVarsBase+0x1C` tick interval:

```text
length      = sqrt(dot(punch, punch) + 1e-10)
next_length = max(length - (0.5 * length + 10) * interval_per_tick, 0)
next_punch  = punch / length * next_length
```

`game::PredictCssPunchDecay` mirrors this. The command pipeline uses the
predicted fire-time punch, both for the recoil stage and for removing the
game's later `+2*punch`. F1 prints current, predicted, and actual fire-time
punch, so the timing stays testable rather than assumed.

### 7.2 Fire adds two punches, the camera adds one

The fire path consumes `cmd + 2*punch`. The x64 player view function
`FUN_180054450` (`client.dll+0x54450`) adds the three punch fields at
`+0x127C/+0x1280/+0x1284` only **once** to the camera angles.

This asymmetry drives the rest of the design:

- exact command no-recoil requires `cmd = desired - 2*punch`;
- if the camera adopted that command, it would show `desired - 2*punch + punch`,
  an inverse recoil of one punch;
- no single command angle can be exact for both fire and camera, so the camera
  must be kept away from the command (section 9).

## 8. Composing the command angle

### 8.1 One composition, not independent writers

Aim, recoil, and spread are composed once in the pure function
`game::ComposeShotAngles`. Separate features must not overwrite each other's
command angles. The toggles change the math, not only which stages run:

| Configuration | `fire_base` | Final command |
| --- | --- | --- |
| no-recoil off, no-spread off | `desired + 2*punch` | `desired` |
| no-recoil on, no-spread off | `desired` | `desired - 2*punch` |
| no-recoil off, no-spread on | `desired + 2*punch` | `inverse_cone(fire_base) - 2*punch` |
| both on | `desired` | `inverse_cone(desired) - 2*punch` |

`punch` is the predicted fire-time punch from section 7.1. With no-recoil off,
no-spread must compensate around the naturally punched fire direction,
otherwise it would silently remove recoil too. The offline tests cover all
four rows.

### 8.2 The inverse cone

The game applies the cone **after** `AngleVectors(fire angles)`. Subtracting
`(sx, sy)` in the intended basis is only a first-order inverse, because the
compensated angles produce a new `forward/right/up` basis. `game::CompensateAngles`
therefore iterates:

```text
inputs: fire_base, (sx, sy)

1. F, R, U = AngleVectors(fire_base)
   c = VectorAngles(normalize(F - R*sx - U*sy))           ; first-order step
2. repeat up to 4 times:
     Fc, Rc, Uc = AngleVectors(c)
     got = normalize(Fc + Rc*sx + Uc*sy)                   ; simulate the game
     if angle(got, F) < 0.001 degrees: stop
     c = VectorAngles(normalize(F - Rc*sx - Uc*sy))       ; re-invert in c's basis
3. residual_deg = angle(post-spread direction of c, F)
4. accept only if residual_deg <= 1 degree
```

The full per-tick sequence in `features/perfect_nospread.cpp` is:

```text
1. desired   = aimbot target, or the post-original command angle
2. punch     = predicted fire-time punch (section 7.1)
3. fire_base = desired + 2*punch, or desired with no-recoil on
4. seed      = stored random_seed if nonzero, else MD5_PseudoRandom(command_number) & 0x7fffffff
5. radii     = fire_inaccuracy (section 5.4) and GetSpread()
6. (sx, sy)  = local replay with RandomSeed((seed & 0xFF) + 1), pellet 0
7. c         = CompensateAngles(fire_base, sx, sy)
8. cmd       = c - 2*punch
```

This is **first-order plus iterative** compensation, not a proven exact
inverse. The 1-degree limit only rejects failures such as a degenerate basis.
The quality bar is far tighter: the loop stops below `0.001` degrees, and a
residual well under `0.01` degrees on a rifle at rest is what "good enough for
this build" means. Neither says the server accepted the shot.

### 8.3 Fail-closed rules

Each stage either applies completely or not at all:

- zero-sequence commands receive no mutation at all (section 4.3);
- the cone is only computed when `perfect_nospread` is on and `IN_ATTACK` is
  set;
- the spread state is usable only when the fire inaccuracy was predicted, the
  spread getter succeeded, and the clip is readable and above zero;
- on x64 the fire-inaccuracy prediction requires a readable accuracy model
  other than `1`, plus the mode, penalty, weapon info, move type, flags, and
  tick interval;
- getter results must be finite and inside `[0, 5]`, and cone radii inside
  `[0, 10]`;
- the seed must resolve (a stored nonzero seed or a positive command number);
- the spread stage requires a readable punch, because it needs the fire-space
  basis;
- the compensated result must pass the residual limit above.

When a stage fails, the command keeps the selected aim plus any readable recoil
stage, and F1 reports that stage as `requested-unavailable`. A partial
inverse-cone result is never written.

## 9. Keeping compensation off the camera

### 9.1 Two angle channels

The client keeps more than one "where am I looking?" value:

| Channel | Owner | Used for |
| --- | --- | --- |
| **Simulation / command** | `CUserCmd::viewangles` | Prediction, weapon fire direction, what the server simulates for the tick |
| **Render / camera** | `VEngineClient014` view angles (`GetViewAngles`/`SetViewAngles`, slots 19/20), plus the punch added during view setup | What appears on screen |

Each attack tick can need a different inverse cone because the seed and the
penalty change. That jitter is correct for the simulation channel. If the
camera also adopts the command angles, the view flicks by the inverse cone on
every shot. That flicker is compensation noise, not recoil.

```text
intended look (player or aimbot)
      |
      v
+---------------------------+   seed, GetInaccuracy, GetSpread, punch
| ComposeShotAngles         | <------------------------------------
| fire space + inverse cone |
+---------------------------+
      |
      +--> cmd.viewangles = compensated fire angle - 2*punch   (simulation)
      |
      +--> CreateMove returns false                            (camera untouched)
      |
      v
prediction and fire use cmd   -> post-spread bullet == intended
caller skips SetViewAngles    -> camera keeps the intended look
```

This is the classic **silent** split: the shot is corrected and the view is
not.

### 9.2 The caller copies the command into the camera only on `true`

Ghidra confirms this in the x64 `client.dll`, not only in the Source SDK.
`FUN_180152290` builds the real ring-buffer command:

```text
+0x1524BD  copy VEngineClient014::GetViewAngles into cmd+0x10 (while alive)
+0x15252F  call ClientMode::CreateMove
           test the boolean return
+0x15254B  only on true: VEngineClient014::SetViewAngles(cmd+0x10)
```

`FUN_1801527B0` repeats the same return-controlled copy for the extra-sample
command at `+0x1528FA..+0x152912`.

The two engine slots, from the `VEngineClient014` vtable at
`engine.dll+0x366070`:

| Slot | Address (sample) | Behavior |
| ---: | --- | --- |
| 19 `GetViewAngles` | `engine.dll+0x70630` | Copies three floats from `engine.dll+0x53E4E4..EC` into the caller's `QAngle` |
| 20 `SetViewAngles` | `engine.dll+0x70F20` | Writes those three globals after per-component normalization (`FUN_1802762F0`, wrap to ±180) |

```asm
; GetViewAngles (slot 19): this = RCX, out QAngle* = RDX
MOVSS XMM0, dword ptr [engine+0x53E4E4]
MOVSS dword ptr [RDX], XMM0
MOVSS XMM1, dword ptr [engine+0x53E4E8]
MOVSS dword ptr [RDX+4], XMM1
MOVSS XMM0, dword ptr [engine+0x53E4EC]
MOVSS dword ptr [RDX+8], XMM0
RET

; SetViewAngles (slot 20): this = RCX, in QAngle* = RDX
; stores AngleNormalize(pitch/yaw/roll) into the same three globals
```

A `SetViewAngles` call made inside the detour is therefore overwritten by the
caller, and a later render-stage restore can race the extra input samples. The
return value is the only stable place to make the split.
`game::SetViewAngles` stays available as a diagnostic-verified interface, but
it is not the silent mechanism.

### 9.3 The return rule

`hooks/create_move.cpp` implements one rule:

```text
intended = cmd.viewangles after the original CreateMove
... bhop, triggerbot, aimbot, ComposeShotAngles ...
if cmd.viewangles differs from intended (any component by more than 1e-4):
    if command no-recoil was applied or silent_angles is on: return false
    else:                                                     return true
otherwise: return the original result
```

Command no-recoil always suppresses the camera copy, even with
`silent_angles=false`, because of the two-versus-one punch asymmetry in section
7.2. For every other mutation (aimbot or no-spread without command no-recoil),
`silent_angles` chooses between a hidden and a visible adjustment.

Two non-silent alternatives were tested and rejected:

- **Copying the exact command** makes the camera show the opposite of the
  recoil (section 7.2).
- **Subtracting punch from the absolute angle every tick** feeds the corrected
  camera into the next command and drives pitch into the `89`-degree clamp.
  Runtime exposed that as `desired.pitch=89` with `residual_deg=9.22407`.

Neither can match both verified paths. An incremental workaround would only
trade the exact fire correction for a different camera error.

### 9.4 Visual no-recoil in `OverrideView`

Visual no-recoil removes the punch that the camera adds. It is read-only with
respect to the player. Ghidra verifies the path:

- x64 ClientMode vtable `client.dll+0x477638`, slot 16 `client.dll+0xE67C0`;
- x86 ClientMode vtable `client.dll+0x397E04`, slot 16 `client.dll+0xF6220`;
- both `OverrideView` implementations access `CViewSetup::origin` at `+0x40`
  and `CViewSetup::angles` at `+0x4C`;
- x64 `CViewRender::SetUpView` at `client.dll+0x1BB5E0` asks the player to
  calculate the view, calls ClientMode slot 16, then builds the render
  matrices;
- the player view function at `client.dll+0x54450` adds one current punch to
  the camera angles before that ClientMode call.

The hook calls the original `OverrideView` first, subtracts one current punch
from `CViewSetup::angles`, and never writes `m_vecPunchAngle`. Chapter 006
captures the resulting view for world-to-screen projection. This layer does
**not** cancel the compensation flicker; that is the return-value split above.

Do not zero the punch around `FrameStageNotify` instead: its stage 5 runs
simulation, and doing so breaks hit accuracy (section 12.6).

### 9.5 What each layer cancels

| Visual problem | Source | Cancellation |
| --- | --- | --- |
| Punch kick on screen | One punch added during the player view calculation | Subtract one punch from `CViewSetup::angles` after `OverrideView` |
| Compensation or silent-aim snap | `cmd.viewangles` adopted as eye angles | Return `false` so the caller skips its camera copy |
| Actual bullet cone | `RandomFloat` in the fire path | Perfect no-spread on the **command only** |

"Zero punch for rendering" and "perfect no-spread" are different things, and
they compose.

| Feature | Hook stage | Interaction with spread and silent |
| --- | --- | --- |
| Bunny hop / triggerbot | `CreateMove` | Buttons only |
| Aimbot | `CreateMove` | Supplies `desired` before fire-space composition |
| Command no-recoil | `CreateMove` | Selects the punch policy of the shared composition |
| Perfect no-spread | `CreateMove` | Inverse cone on the command |
| Silent camera | `CreateMove` return value | `false` when the command angles changed |
| Visual no-recoil | `ClientMode::OverrideView` | Read-only camera punch removal after the original call |
| F1 debug | `CreateMove`, after composition | Stage flags, angles, seed, radii, residual |

### 9.6 Optional: local FX cosmetics

Even with exact simulation compensation, tracers built by `FX_FireBullets`
still sample the cone. As a local-only cosmetic you could force the local FX
radii to zero, but that has nothing to do with hit registration and is not a
substitute for command compensation. It is not implemented.

## 10. Configuration

INI keys under `[features]` in `config/signatures.ini` and
`config/signatures-x64.ini`, loaded into `features::Config`:

| INI key | Runtime field | Default | Meaning |
| --- | --- | --- | --- |
| `perfect_nospread` | `perfectNoSpread` | `false` | Fire-space first-order plus iterative command compensation |
| `norecoil` | `norecoil` | `true` | Include punch cancellation in the shared composition |
| `silent_angles` | `silentAngles` | `true` | Suppress camera copies for aim and no-spread mutations; command no-recoil always suppresses its own |
| `visual_norecoil` | `visualNoRecoil` | `true` | Read-only camera correction in ClientMode slot 16 |
| (fixed) | `pellet0` | | Multi-pellet policy (section 6.4); not configurable |

Keep `perfect_nospread=false` until the F1 residuals and the fire-time capture
look right for the build under test.

## 11. Diagnostics

### 11.1 The F1 block

F1 prints this after composition in `CreateMove`, so it shows the final values
for that tick:

```text
spread diag: weapon=0x... handle=0x...
  accuracy dispatch: inaccuracy_method=0x... spread_method=0x...
  accuracy model: weapon_accuracy_model parent_slot=0x... convar=0x... value=... weapon+0xca8=... (1=unmodeled, other=modeled)
  mode=... weaponId=... weaponInfoIndex=... shotsFired=... clip=... penalty=... nextPenalty=...
  pre-fire decay=... baseline=... recovery=... interval=...
  GetInaccuracy=... fireInaccuracy=... GetSpread=... methodsOk=... fireRadii=(inaccuracy=...,spread=...) (polar seed8+1) next_accuracy=yes|no usableForCompensation=...
  cmd.number=... stored_seed=... effective_seed=... seed8=... IN_ATTACK=...
  angles cmd=(...) engine=(...) delta=(...)
  shot pipeline: aim=input|target recoil=off|applied|requested-unavailable spread=off|applied|requested-unavailable attack=yes|no
  shot angles: desired=(...) fire_base=(...) cmd=(...) cmd_fire=(...)
  punch current=(...) predicted_fire=(...) interval=...
  predicted pellet0 sx=... sy=... seed8=... sampler_seed=... residual_deg=... iters=... (CS polar fire-space, first-order+iterative)
  perfect_nospread=on|off silent_angles=on|off pellet_policy=pellet0
  client_fire_capture=waiting_for_attack_command|waiting_for_matching_fire_call|idle
```

How to read it:

- `accuracy model` is the `weapon_accuracy_model` read from section 5.3.
  `value=unavailable` means the getter prologue did not match the expected
  shape, and the pre-fire prediction fails closed.
- `GetInaccuracy` is the hook-time getter value, and `fireInaccuracy` is the
  predicted fire-time radius after the pre-fire decay. Only the second is used.
- `nextPenalty` is the post-shot penalty (section 5.7), shown for comparison.
- `cmd_fire` is `cmd + 2*predicted punch`: the angle the fire path should
  receive.
- With camera suppression active, a healthy fire tick shows `cmd` diverging
  from `engine` while the camera stays on the intended look.
- `methodsOk=yes` and a nonzero `weapon=` are the proof that the handle lookup
  worked. A handle value alone is not (chapter 004, section 7.1).

### 11.2 The fire-time capture

F1 also arms two x64 diagnostic detours: `FX_FireBullets` (through the
`ClientFireBullets` call-site signature) and `UpdateAccuracyPenalty` (slot
384). The next matching local fire call prints:

```text
client fire-time diag:
  player=... weaponId=... weaponInfoIndex=... mode=... command=... accuracy_model=...
  seed expected8=... actual8=... match=...
  radii create_move_getter=(...) predicted_fire_inaccuracy=... actual=(...) delta=(...) predicted_delta=(...)
  accuracy decay=... baseline=... recovery=... interval=...
  angles cmd=(...) expected_fire=(...) actual_fire=(...) delta=(...)
  update_accuracy calls=... penalty_before=... penalty_after=... interval_before=... interval_after=...
  inaccuracy components create_penalty=... create_base=... actual_penalty=... actual_base=...
  movement create=speed2d=... flags=... movetype=... actual=speed2d=... flags=... movetype=...
  punch create=(...) predicted_fire=(...) actual=(...)
  fire source=(...) reconstructed=(...) delta=(...)
  origin=(...) sound_time=...
  offsets create_move=(...) actual_args_replay=(...)
  final direction target=(...) residual_deg=...
```

This block is the acceptance evidence: `predicted_delta`, the punch
prediction, the reconstructed fire angle, and the cone offsets must agree with
the actual call before a build is considered validated.

## 12. How the model was validated

The final model above took several passes. Each wrong turn is a reusable
lesson, and the captures below are the evidence for the current design.

### 12.1 First pass: the wrong fire path

The first search stopped at the generic `CShotManipulator`-shaped functions
(`client.dll+0x4FF30`, `server.dll+0x137890`). The predictor replayed a
valid-looking but different RNG and vector path, and the result was misleading:
tracers looked straight locally while real shots went wide. The second pass
followed the actual CS weapon call from the getter slots into the client and
server `FX_FireBullets` implementations (section 6.2).

That pass also removed an inference that treated `GetInaccuracy()+GetSpread()`
as one combined radius with equal components. The CS path samples the two radii
separately. That was the key correction for a burst capture that reported a
valid weapon, seed, and residual while bullets still went wide.

### 12.2 The `CreateMove` getter is one tick early

With the right fire path, a captured AK command matched the seed and the spread
radius exactly, but not the inaccuracy:

```text
CreateMove GetInaccuracy: 0.0291354
fire-time inaccuracy:     0.0277709
expected fire angle:      (-3.49101, -85.3237)
actual fire angle:        (-3.11862, -85.4470)
```

Ghidra showed two real updates between those observations: the slot-384
penalty decay (section 5.6) and the CSS punch decay (section 7.1). Both are
now predicted.

### 12.3 Separating movement from penalty

A paired stationary/moving capture isolated what was left:

| Capture | CreateMove base | Fire-time base | Predicted penalty | Fire-time penalty |
| --- | ---: | ---: | ---: | ---: |
| stationary | `0` | `0` | `0.0280808` | `0.0289072` |
| moving at about 221 units/s | `0.09222` | `0.09222` | `0.0280808` | `0.0289072` |

The movement term matched exactly in both captures, and so did the seed, the
spread radius, the predicted punch, and the reconstructed fire angle. The same
`0.00082645` penalty error remained in both, so the cause was not
changing-movement prediction.

### 12.4 Right equation, wrong weapon-info record

The optional `UpdateAccuracyPenalty` detour then ruled out timing. One capture
reported the same penalty at `CreateMove` and at slot-384 entry,
`interval_before=interval_after=0.015`, and the exact slot-384 output:

```text
penalty_before=0.0296666
penalty_after=0.0282658
predicted=0.0274732
```

The equation was right but its inputs came from the wrong weapon-info record.
The implementation had passed the virtual weapon id (slot 371) to the lookup,
while the game uses the `ushort` at `weapon+0xC62` (section 5.5). The same
listing also corrected the recovery fields: standing on the ground and ladder
use `+0x91C`; crouching and airborne use `+0x920`.

### 12.5 The matching capture

After the fix, fire-call weapon id 27 used accuracy-info index 46. The
predictor read baseline `0.00916`, recovery `0.48815`, and interval `0.015`, and
matched slot 384 and the fire call exactly:

```text
predicted_fire_inaccuracy=0.0282658
actual_inaccuracy=0.0282658
predicted_delta=0
penalty_before=0.0296666
penalty_after=0.0282658
expected_fire=(-4.36307,-74.4289)
actual_fire=(-4.36307,-74.4289,0)
angle_delta=(9.53674e-07,0)
offsets_create_move=(0.00615941,0.00602059)
offsets_actual_args_replay=(0.00615941,0.00602059)
```

This validates the client-side seed, the one-tick penalty decay, the punch
decay, the fire-angle basis, the radii, and the polar replay for that shot. It
does not by itself prove the authoritative server shot or the physical impact.
Local bot testing afterwards reported accurate hits with command no-recoil and
perfect no-spread enabled.

### 12.6 Zeroing punch in `FrameStageNotify` broke accuracy

The first visual no-recoil implementation zeroed `m_vecPunchAngle` around
`FrameStageNotify(FRAME_RENDER_START)` and restored it afterwards. With it
enabled, otherwise exact command no-recoil plus no-spread started to miss.

Ghidra explains why. x64 `FrameStageNotify` at `client.dll+0xD59B0` dispatches
stage 5 to `client.dll+0xD6D70`, which runs `Client SimulateEntities`,
temporary-entity simulation, and particle simulation. Stage 5 is not a
render-only window. Writing punch there changes state that simulation consumes,
and restoring an older value afterwards can overwrite a legitimate update. The
replacement is the read-only `OverrideView` correction in section 9.4.

### 12.7 Judging a wall test

A wall test with only perfect no-spread enabled does not produce a tight
cluster, because natural punch still moves the fire angle. In the captured
shot, `2*predicted_punch` contributed about 9.3 degrees, while the cone offset
was about 0.13 degrees. Either compare each impact with that shot's logged
recoil-adjusted fire angle, or enable command no-recoil as well when the
expected result is one compact cluster. Visual no-recoil only changes
rendering and does not affect this test.

## 13. Runtime validation checklist

Observed on the x64 sample build:

- [x] x64 process and x64 `client.dll`
- [x] `m_hActiveWeapon` netvar resolves, and the handle resolves to a non-null weapon
- [x] the weapon vtable is readable, and slots `0xBF0` / `0xBF8` / `0xB98` are executable
- [x] `WeaponInfoLookup` has one match and returns readable data for the live index
- [x] `GetInaccuracy` / `GetSpread` return finite, plausible values (near 0 when
      standing still after recovery, larger when moving or mid-spray)
- [x] the accuracy model is reported; model `2` uses the predicted pre-fire
      decay, and model `1` fails closed
- [x] fire-time seed, predicted pre-fire radii, and angles match the captured command
- [x] the slot-384 detour reports the exact penalty and interval before and after the update
- [x] predicted punch equals the actual `player+0x127C` value at fire time
- [x] `fire source + 2*actual punch` reconstructs the hooked fire angle
- [x] the CS polar replay uses `seed8+1`, and its offline vectors match the local
      ran1-style stream and angle/radius draw order
- [x] `m_fAccuracyPenalty` rises after firing and decays over time
- [x] positive `command_number` values change while firing, and
      `effective_seed` matches the seed the game publishes after `ClientMode::CreateMove`
- [x] zero-sequence extra samples are reported as unavailable and receive no mutation

The fail-closed rules in section 8.3 are enforced in code. They are not a
runtime observation, and not every failure branch has been triggered in a live
session.

## 14. Signatures used by this chapter

Prefer netvars for weapon fields (`m_fAccuracyPenalty`, `m_weaponMode`,
`m_hActiveWeapon`, `m_iClip1`) and virtual calls on the live weapon for the
getters. The remaining x64 values have no named interface and use configured
signatures:

| Signature | Resolves | August match | 2026-09-20 match |
| --- | --- | ---: | ---: |
| `WeaponInfoLookup` | the weapon-info lookup function entry (`operand=match`) | `client.dll+0x1DC630` | `client.dll+0x1DB800` |
| `ClientFireBullets` | the `FX_FireBullets` call site and its rel32 target | `+0x236C90` → `+0x1FFA30` | `+0x235E80` → `+0x1FEC00` |
| `GlobalVars` | the `CGlobalVarsBase` pointer slot inside `UpdateAccuracyPenalty` | `client.dll+0x2368E4` | `client.dll+0x235AD4` |
| `UpdateAccuracyPenalty` | the slot-384 function entry, for the optional diagnostic detour | `client.dll+0x2367D0` | `client.dll+0x2359C0` |

All four still matched uniquely after the 2026-09-20 update (chapter 004,
section 11.1). Their targets were cross-checked statically on the new build:
the new `FX_FireBullets` loads the `2π` constant, and the `weapon+0xC62`
accuracy helper at `client.dll+0x4D4B0` still tail-jumps to the new
`WeaponInfoLookup` match. Do not hardcode Ghidra absolute addresses into the
DLL. Log module-relative matches the same way as ClientMode and ClientState.

## 15. x86 cross-check

Same CS:S client as PE32 (`x86:LE:32:default`, image base `0x10000000`).

### 15.1 Netvars

| Property | Table | x86 offset | x64 offset |
| --- | --- | ---: | ---: |
| `m_hActiveWeapon` | `DT_BaseCombatCharacter` | `0xD80` | `0x11A0` |
| `m_iShotsFired` | `DT_CSLocalPlayerExclusive` | `0x1540` | `0x1A34` |
| `m_weaponMode` | `DT_WeaponCSBase` | `0x92C` | `0xCAC` |
| `m_fAccuracyPenalty` | `DT_WeaponCSBase` | `0x930` | `0xCB0` |
| `m_iClip1` | `DT_LocalWeaponData` | `0x8BC` | `0xC30` |

The offsets differ because of pointer padding. Use runtime
`netvars::GetOffset` for both builds.

### 15.2 Vtable slots are stable, byte offsets are not

| Method | Slot | x86 byte offset | x64 byte offset |
| --- | ---: | ---: | ---: |
| `GetInaccuracy` | **382** | `0x5F8` | `0xBF0` |
| `GetSpread` | **383** | `0x5FC` | `0xBF8` |

Evidence: x86 fire `FUN_1020B490` calls `[EAX+0x5F8]` / `[EAX+0x5FC]`, while
x64 fire uses `[RAX+0xBF0]` / `[RAX+0xBF8]`. Same slot index, different pointer
width.

### 15.3 Active weapon and weapon info

| Arch | Handle resolver | Handle offset | List stride | Weapon-info lookup | Table stride |
| --- | --- | ---: | ---: | --- | ---: |
| x86 | `FUN_10073BA0` | `+0xD80` | `0x10` | `client.dll+0x1C0C50` | `0x10` |
| x64 | `FUN_18004A890` | `+0x11A0` | `0x20` | `client.dll+0x1DC630` | `0x18` |

Both resolvers use `index = handle & 0xFFFF`, serial `handle >> 16`, and return
null on a serial mismatch. The x86 weapon-info fields are `AccuracyQuadratic`
`0x89C`, `AccuracyDivisor` `0x8A0`, `AccuracyOffset` `0x8A4`, and
`MaxInaccuracy` `0x8A8`.

### 15.4 What x86 does not model

- The x86 build does not model the pre-fire decay or the accuracy model: it
  uses the hook-time `GetInaccuracy()` as the fire radius, and it passes the
  virtual weapon id as the weapon-info index.
- x86 weapon fire also takes the low byte of the seed global before calling
  `FX_FireBullets` (`client.dll+0x1DDA90`), which uses the same two-radius
  idea. The x86 helper, the server copy, the seed increment, and the pair
  orientation still have to be checked independently. Do not copy the x64
  evidence.
- The punch-decay prediction is x64-only.

x86 compensation is considered validated only after its own instruction-order
and runtime checks. The x86 profile keeps `perfect_nospread=false`.

## 16. Status and boundary

| Item | Status |
| --- | --- |
| Weapon resolve and `GetInaccuracy`/`GetSpread` | Implemented with bounds; the x64 handle is passed as a `CBaseHandle` (chapter 004, section 7.1) |
| Accuracy model (`weapon_accuracy_model`) read | Implemented; model `1` fails closed |
| Pre-fire accuracy decay | Validated against slot 384 / `client.dll+0x2367D0` |
| Pre-fire CSS punch decay | Validated against movement slot 15 / `client.dll+0x1F5AE0` |
| CS polar cone prediction | Implemented: `seed8+1`, separate radii, local stream |
| Generic `CShotManipulator` path | Documented as a separate path; not used |
| First-order plus iterative compensation | Implemented |
| Config-aware aim/recoil/spread composition | Implemented; offline tests cover every toggle combination |
| Silent camera | Implemented through the `CreateMove` return value |
| Visual no-recoil | Implemented in ClientMode slot 16; player punch stays read-only |
| Fire-time argument capture | Implemented on x64 through the verified `FX_FireBullets` call target |
| `game::SetViewAngles` (slot 20) | Diagnostic only |
| Offline tests | `tests/weapon_math_tests.cpp`, including the penalty-decay table; CTest passes on x86 and x64 |

The x64 sample satisfies the static-evidence, checked-read, diagnostic, build,
offline-test, and local-smoke-test gates. The feature stays labeled
**exploratory** because these are not validated:

- the `weapon_accuracy_model 1` path;
- the x86 live cone path and pre-fire timing;
- a runtime pass on the 2026-09-20 build (its signatures, slots, and
  functions are verified statically in chapter 004, section 11.1);
- authoritative remote-server behavior.

### Reversal record (summary)

```text
Question/behavior: perfect no-spread (CS polar inverse cone on the command) + silent camera
Binaries: client.dll x64 + x86; server.dll x64; engine.dll x64 for the view path
Ghidra: /source-engine-tutorial/x64/client.dll and server.dll.1 (0x180000000), /client.dll (0x10000000)
Seed: MD5_PseudoRandom(command_number) & 0x7fffffff, low byte, +1 inside the fire path
CS cone: client.dll+0x1FFA30 / server.dll+0x3252E0; one inaccuracy sample, one spread sample per pellet; pellet0
Fire radii: one slot-384 penalty decay before fire, weapon-info index from weapon+0xC62, spread from GetSpread
Accuracy model: weapon_accuracy_model (replicated ConVar, default 2); model 1 fails closed
Punch: CSS movement slot 15 at client.dll+0x1F5AE0 decays one tick before the wrapper reads player+0x127C
Composition: desired -> config-selected fire_base -> inverse cone -> minus 2*predicted punch
Silent: always suppress the camera copy for command no-recoil; silent_angles for other mutations
Visual punch: read-only subtraction in OverrideView; never write punch during frame stages
Not this path: client.dll+0x4FF30 / server.dll+0x137890 (generic FireBullets), CTEFireBullets replay
Config: perfect_nospread=false; silent_angles=true
Runtime: x64 fire-time capture matched the command; x86 cone and model 1 remain open
```
