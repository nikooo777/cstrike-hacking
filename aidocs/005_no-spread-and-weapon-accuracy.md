# 005 — No-spread exploration: weapon accuracy, seed, and fire path

This chapter records a **read-first** reverse of the current x64 CS:S client
and server CS bullet-cone paths. The important correction from the first pass
is that this build's current CS fire path is the polar sampler reached by
`FX_FireBullets`: it uses separate `GetInaccuracy()` and `GetSpread()` radii
and calls `RandomSeed(seed8 + 1)`. The generic `CShotManipulator`-shaped
functions found during the initial search are a different `FireBullets`
path and are not the predictor target for this weapon pipeline.

The implementation remains guarded and exploratory: matching a local/client
path does not prove that a remote server will accept a compensated shot. The
goal is to know **which subsystem owns each value**, which values are
networked versus client-only, and what a safe diagnostic should print when a
stage cannot be validated.

Use this only against binaries and processes you are permitted to study,
preferably offline / local. Client-side angle tricks that “look perfect” can
still disagree with a server that recomputes hits independently.

Program used for this pass:

| Item | Value |
| --- | --- |
| Module | `client.dll` (path `cstrike/bin/x64/client.dll`) |
| Ghidra program | `/source-engine-tutorial/x64/client.dll` |
| Language | `x86:LE:64:default` |
| Image base | `0x180000000` |

The server comparison used `/source-engine-tutorial/x64/server.dll.1`
(`x86:LE:64:default`, image base `0x180000000`). The similarly named
`server.dll` Ghidra import was malformed for this pass; the current on-disk
server binary was checked separately to confirm the same CS polar sequence.

Cross-check sources: Source SDK 2013 (`in_main.cpp` seed assignment,
`MD5_PseudoRandom`), RecvTable string anchors in this client, and the already
verified `CreateMove` path from [004](004_x64-migration-and-abi.md).

## 1. What “no-spread” is *not*

Do not start by searching for an old “no-spread offset” from a public list.
For this client the spread path is a **pipeline**:

```text
CreateMove fills CUserCmd
  -> command_number -> MD5_PseudoRandom -> random_seed (after ClientMode::CreateMove)
  -> prediction RunCommand publishes that seed to a client global
  -> weapon primary-fire reads GetInaccuracy / GetSpread
  -> FX_FireBullets(seed8, inaccuracy, spread) drives the current CS polar cone
  -> current client/server CS path adds the inaccuracy and pellet spread samples
  -> FireBullet builds forward + right*sx + up*sy and traces the shot
  -> optional CTEFireBullets replicates seed + cone values for other clients
```

“No-spread” could mean any of:

1. **Visual-only**: zero the cone inside `FX_FireBullets` or force
   `GetInaccuracy`/`GetSpread` to `0` on the local client.
2. **Angle compensation (“perfect no-spread”)**: leave the CS fire path
   alone, reverse its deterministic seed/vector samples, and pre-adjust
   `CUserCmd::viewangles` so the *post-spread* direction matches the intended
   aim.
3. **State wipe**: zero `m_fAccuracyPenalty` or related weapon fields (partial;
   movement / stand-crouch tables still contribute).

**Target for this tutorial series:** (2) **perfect no-spread**, plus a
**silent visual layer** so the local camera does not show the compensated
angles shaking. Crude zeroing (1)/(3) stays documented only as a contrast.

Each approach has different observability and different server interaction.
The rest of this chapter records the fire pipeline, then the sim-vs-render
angle split needed for silent perfect nospread.

## 2. Semantic anchors found in Ghidra

### 2.1 Strings (RecvTables, weapon data, FX)

| String | Address | Role |
| --- | ---: | --- |
| `DT_TEFireBullets` | `client.dll+0x475068` | Temp-entity table for bullet FX |
| `m_fInaccuracy` / `m_fSpread` / `m_iSeed` | near `+0x4750d0` | Fields on that TE |
| `FX_FireBullets: weapon alias...` | `+0x47d698` | Error path inside `FX_FireBullets` |
| `Spread`, `InaccuracyStand`, … | near `+0x47d328` | Weapon script keys |
| `weapon_accuracy_logging` | `+0x47d680` | ConVar registration |
| `weapon_accuracy_model` | `+0x4ac628` | ConVar registration |
| `m_fAccuracyPenalty` | `+0x4adb28` | `DT_WeaponCSBase` netvar |
| `m_weaponMode` | `+0x4adb18` | `DT_WeaponCSBase` netvar |
| `m_hActiveWeapon` | `+0x400fe8` | `DT_BaseCombatCharacter` |
| `m_iClip1` | `+0x3f7ad8` | `DT_LocalWeaponData` |
| RTTI `.?AVC_WeaponCSBase@@` | `+0x5db3e8` | Weapon class identity |

### 2.2 Netvar offsets from RecvProp registration (this build)

From the RecvTable constructors:

| Table | Property | Offset |
| --- | --- | ---: |
| `DT_WeaponCSBase` | `m_weaponMode` | `0xCAC` |
| `DT_WeaponCSBase` | `m_fAccuracyPenalty` | `0xCB0` |
| `DT_BaseCombatCharacter` | `m_hActiveWeapon` | `0x11A0` |
| `DT_LocalWeaponData` | `m_iClip1` | `0xC30` |
| `DT_TEFireBullets` | `m_iSeed` | `0x44` (RecvTable / networked-data base) |
| `DT_TEFireBullets` | `m_fInaccuracy` | `0x48` |
| `DT_TEFireBullets` | `m_fSpread` | `0x4C` |

These RecvProp displacements are relative to the **networked data base** of
the temp entity (the layout `FUN_18001DF00` registers). They are **not**
automatically the same as displacements from every `this` pointer that
touches the TE.

`CTEFireBullets` PostDataUpdate (`FUN_1801F0500`) uses a different basis: its
`this` is eight bytes earlier than the RecvTable base (vtable / `C_BaseTempEntity`
header). On that object pointer:

| Field | RecvProp (data base) | PostDataUpdate `this` |
| --- | ---: | ---: |
| `m_vecOrigin` | `+0x24` | `+0x1C` |
| `m_iSeed` | `+0x44` | `+0x3C` |
| `m_fInaccuracy` | `+0x48` | `+0x40` |
| `m_fSpread` | `+0x4C` | `+0x44` |

Both numbers are valid for their pointer basis. Prefer
`netvars::GetOffset` when walking RecvTables; when reading listing
displacements, record **which `this`**.

### 2.3 Weapon script table (`CCSWeaponInfo` parse)

`FUN_1801fdc30` (`client.dll+0x1FDC30`) loads script keys into a weapon-info
object. Relevant displacements on that object (not the weapon entity):

| Key | Offset |
| --- | ---: |
| `MaxPlayerSpeed` | `+0x728` |
| `AccuracyQuadratic` | `+0x8CC` |
| `AccuracyDivisor` | `+0x8D0` |
| `AccuracyOffset` | `+0x8D4` |
| `MaxInaccuracy` | `+0x8D8` |
| `Spread` / `SpreadAlt` | `+0x8DC` / `+0x8E0` |
| `InaccuracyCrouch` / `Stand` / `Jump` / … | `+0x8E4` … |
| `InaccuracyFire` (+ alt) | `+0x90C` / `+0x910` |
| `InaccuracyMove` (+ alt) | `+0x914` / `+0x918` |
| `Bullets` | `+0x8C4` |

Primary fire indexes many of these with `m_weaponMode` as a `0/1` table
selector.

## 3. `CUserCmd` and the prediction seed

### 3.1 x64 layout (MSVC)

The existing `sdk/user_cmd.h` declaration matches the live prediction code when
the vtable pointer is accounted for:

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

Evidence: prediction helper `FUN_1800524c0` stores
`*(cmd + 0x38)` into global `DAT_1805a6000` (`client.dll+0x5A6000`).
`FUN_180187440` (RunCommand-style) calls that helper at command start and
clears the seed again at the end.

Weapon fire then does:

```text
MOV   EBX, dword ptr [client.dll+0x5A6000]  ; full int from cmd.random_seed
MOVZX ESI, BL                               ; seed8 = random_seed & 0xFF
; ... pass ESI into FX_FireBullets as the seed argument
```

So **this build’s local fire path only uses the low 8 bits** of the published
seed when calling `FX_FireBullets`. Perfect-nospread math must use the same
mask, not the full 31-bit value, unless a later re-dump shows a different
path.

SDK cross-check (Source SDK 2013 `in_main.cpp`):

```cpp
cmd->random_seed = MD5_PseudoRandom( sequence_number ) & 0x7fffffff;
```

The seed is still **deterministic from the command sequence**, not free
entropy. That is what makes angle compensation *possible in principle* on a
matching client/server RNG path. Imports used by the cone are vstdlib’s
`RandomSeed` / `RandomFloat` (IAT at `client.dll+0x3E15D8` / `+0x3E15E0` in
this sample).

At this hook point, the field is normally still zero because the assignment is
performed after `ClientMode::CreateMove` returns. The implementation therefore
derives the effective seed from `command_number` with a local one-block
`MD5_PseudoRandom` implementation and uses that value for cone prediction.

There is a second input path to account for. Ghidra shows
`FUN_1801527B0` calling the same `ClientMode::CreateMove` slot with a temporary
stack `CUserCmd`; its `command_number` is left at zero. The normal command path
(`FUN_180152290`) writes `command_number = sequence_number` at `cmd + 0x08`,
calls `ClientMode::CreateMove`, and only then writes `random_seed` at `cmd +
0x38`. Therefore `MD5_PseudoRandom(0)` must not be treated as a real firing
seed: a zero-sequence command is an extra mouse sample. The hook now skips all
feature mutation on it, not only compensation, and defers an F1 dump if the
key edge lands there. A valid runtime dump should show positive command
numbers while firing, `seed_source=command_number` at this hook, and a
changing `seed8`; `effective_seed=unavailable seed_source=zero_sequence`
identifies a sample that must not be used for firing.

### 3.2 Compile-time layout check recommendation

Add `static_assert` offsets for `random_seed` (and other fields used by
features) so x86/x64 packing cannot silently drift. Do not assume an x86
overlay without re-measuring.

## 4. Fire path: `GetInaccuracy` → `GetSpread` → `FX_FireBullets`

### 4.1 Example client weapon/FX wrapper

`FUN_180236ab0` (`client.dll+0x236AB0`) is the client weapon/FX wrapper.
It updates the local accuracy state and then feeds the cosmetic effect path;
it is not the shared hit-trace function. Listing evidence (weapon `this` in
`R15`, owner player in `RBP`):

```text
shots = ++player->m_iShotsFired
accuracy_state = shots^2 (or shots^3) / AccuracyDivisor + AccuracyOffset
weapon+0xCA8 = min(accuracy_state, MaxInaccuracy)

; virtual GetSpread
CALL   qword ptr [RAX + 0xBF8]   ; float in XMM0 → spread
; virtual GetInaccuracy
CALL   qword ptr [RAX + 0xBF0]   ; float in XMM0 → inaccuracy

MOV    EBX, dword ptr [client.dll+0x5A6000]  ; prediction seed global
MOVZX  ESI, BL

CALL   FX_FireBullets(... seed=ESI, inaccuracy, spread ...)

; only after FX_FireBullets returns:
weapon+0xCB0 += weapon_info[mode].accuracy_penalty_per_shot
```

Weapon-entity fields touched here:

| Field | Offset | Notes |
| --- | ---: | --- |
| clip (`m_iClip1`) | `0xC30` | decremented on fire |
| mode (`m_weaponMode`) | `0xCAC` | alt-fire table index |
| accuracy scalar | `0xCA8` | derived, client calc |
| accuracy penalty | `0xCB0` | netvar `m_fAccuracyPenalty` |

Player field for shots-fired scaling is **`m_iShotsFired` at `+0x1A34`**,
registered on `DT_CSLocalPlayerExclusive` in `FUN_18001D2E0`. That matches the
fire path’s `player+0x1A34` access; prefer the netvar name at runtime.

### 4.2 Active weapon handle → pointer

`m_hActiveWeapon` lives at player `+0x11A0`. The client’s own resolver
(`FUN_18004A890`, `client.dll+0x4A890`) is the reference implementation:

```text
handle = *(uint32*)(player + 0x11A0)
if handle == 0: return null

index  = (handle == 0xFFFFFFFF) ? 0x1FFF : (handle & 0xFFFF)
entry  = entity_list_base + index * 0x20 + 8
if entry.serial != (handle >> 16): return null
return entry.pointer
```

Notes for the tutorial:

- Low **16** bits are the list index; high **16** bits are the serial.
- Stride `0x20` is the internal list record size, not `IClientEntityList`’s
  public API layout. Prefer calling a verified `GetActiveWeapon`-style path or
  reusing this serial check; do not invent a different mask without re-dump.
- `entity_list_base` is `PTR_DAT_1805ae9f8` in this sample (resolve via the
  same data flow, not a hardcoded ASLR address).

### 4.2.1 Weapon-info lookup and next-shot accuracy

The x64 lookup is `FUN_1801DC630` (`client.dll+0x1DC630`). Ghidra shows a
`ushort` weapon-id argument, a `0x18`-byte table stride, and the selected
weapon-info pointer at table entry `+0x10`; invalid ids return a fallback
object. The x86 analogue is `FUN_101C0C50` (`client.dll+0x1C0C50`) with the
same logical lookup and a `0x10`-byte table stride.

The fire wrapper reads these x64 weapon-info fields before calling the getter
slots:

| Field | x86 | x64 |
| --- | ---: | ---: |
| `AccuracyQuadratic` | `0x89C` | `0x8CC` |
| `AccuracyDivisor` | `0x8A0` | `0x8D0` |
| `AccuracyOffset` | `0x8A4` | `0x8D4` |
| `MaxInaccuracy` | `0x8A8` | `0x8D8` |

The byte at `AccuracyQuadratic` selects square (`nonzero`) versus cubic
(`zero`) shot scaling. The configured `WeaponInfoLookup` pattern resolves the
function entry itself with `operand=match`; it is optional so a changed build
fails closed instead of calling an unverified helper. Runtime diagnostics now
show `weaponId`, `shotsFired`, `nextPenalty`, and `fireInaccuracy` separately.
The wildcarded Ghidra byte searches are unique in both sample clients:
`client.dll+0x1DC630` on x64 and `client.dll+0x1C0C50` on x86.

### 4.3 Virtual methods on the weapon

| Method | Vtable byte offset | Slot (÷8) | Implementation (sample) |
| --- | ---: | ---: | --- |
| `GetInaccuracy` | `0xBF0` | 382 | `FUN_180232900`; live branch 2 enters `FUN_180235B10` |
| `GetSpread` | `0xBF8` | 383 | `FUN_180235D90` |

`GetInaccuracy` combines:

- movement speed vs weapon max speed thresholds,
- `InaccuracyMove[mode]` from weapon info (`+0x914` + `4*mode`),
- plus `m_fAccuracyPenalty` at `weapon+0xCB0`.

The slot-382 entry first checks a runtime accuracy-model selector. The captured
AK reports selector value **2**, so it takes the base path above. Selector value
1 is a separate special path using `weapon+0xCA8` with player-state-dependent
coefficients; that path is not modeled by the current compensation and must
fail closed if observed.

`GetSpread` returns `0` when `weapon_accuracy_model == 1`, otherwise
`Spread[mode]` from weapon info (`+0x8DC` + `4*mode`).

These slots are for **this** build’s `C_WeaponCSBase` layout. Re-verify if the
vtable grows; do not treat 382/383 as eternal constants.

### 4.4 `FX_FireBullets` cosmetic cone math (exact for this binary)

Function: `FUN_1801FFA30` (`client.dll+0x1FFA30`).
Imports: `RandomSeed`, `RandomFloat` from vstdlib.
Helper: `FUN_18038F140` builds a two-float pair via `sin(theta)` and
`sin(theta + pi/2)` (= `cos(theta)`). The pair is returned as
`(low, high) = (sin(theta), cos(theta))`; the current x64 fire call consumes
the high lane as right and the low lane as up, so `(right, up) =
(cos(theta), sin(theta))`. Verify both the helper return order and the
consumer shuffle again after a binary update.
Constant at `client.dll+0x3FB6E8` = `0x40C90FDB` = **2π**.

Listing-backed sequence when a local shooter object exists:

```text
; weapon path: seed_argument is already (random_seed & 0xFF)
; TE path: seed_argument is the networked m_iSeed (full int; see §4.7)
RandomSeed(seed_argument + 1)

; shared inaccuracy sample (once per shot)
θ0 = RandomFloat(0.0, 2π)
r0 = RandomFloat(0.0, inaccuracy)
sx0 = cos(θ0) * r0                    ; right-plane offset
sy0 = sin(θ0) * r0                    ; up-plane offset

; per pellet (weapon info Bullets; x64 CCSWeaponInfo +0x8C4)
for i in 0 .. bullets-1:
    θ  = RandomFloat(0.0, 2π)
    r  = RandomFloat(0.0, spread)
    sxi = cos(θ) * r
    syi = sin(θ) * r
    sx  = sx0 + sxi
    sy  = sy0 + syi
    FireBullet(origin, angles, ..., sx, sy)
```

x86 `FX_FireBullets` (`client.dll+0x1DDA90`, image base `0x10000000`) uses the
same two-radius idea. Its helper/instruction order must still be checked
independently; the x64 implementation is the evidence used by the current
predictor.

### 4.5 `FireBullet` direction construction

`FUN_1801F9BB0` applies the offsets after `AngleVectors` on the aim angles
(`FUN_1802AEA60`, degrees → radians with `* 0.017453292`):

```text
AngleVectors(angles, forward, right, up)
dir = forward + right * sx + up * sy
dir = normalize(dir)
; then trace along dir
```

The same direction construction is used by the current CS fire path. Therefore
the **simulation channel** that perfect nospread must invert is:

```text
intended_forward
  = normalize( AngleVectors(cmd_angles).forward
               + right * sx + up * sy )
```

with `(sx, sy)` produced by the polar RNG sequence above for the same seed,
inaccuracy, and spread values the weapon will use on that tick.

### 4.6 Inverse for perfect nospread (first-order + iteration)

The game applies spread **after** `AngleVectors(cmd.viewangles)`. A single
subtraction of `(sx, sy)` on the **intended** basis is therefore only a
**first-order** inverse: after you write compensated angles, the engine
recomputes a new `forward/right/up` and the residual is not identically zero.

Tutorial implementation (`game::CompensateAngles`):

```text
1. fire_base = desired aim plus the configured recoil policy
   (natural +2*punch when no-recoil is off)
2. seed     = MD5_PseudoRandom(cmd.command_number) & 0x7fffffff
   seed8    = seed & 0xFF
3. fire radii for the observed branch-2 AK = live `GetInaccuracy()` and
   `GetSpread()` (separate values). `+0xCB0` is updated only after the fire call,
   so no next-penalty delta belongs in this shot.
4. `(sx, sy)` = local polar replay with `RandomSeed(seed8 + 1)`:
   one inaccuracy sample plus one spread sample for pellet 0
   The static client/server evidence for this sequence is in §4.10.
5. First-order: aim = normalize(F_i - R_i*sx - U_i*sy) on fire_base
6. compensated_fire = VectorAngles(aim)
7. Iterate a few times using the **compensated fire** basis:
     got = normalize(F_c + R_c*sx + U_c*sy)
     aim = normalize(F_fire_base - R_c*sx - U_c*sy)
     compensated_fire = VectorAngles(aim)
8. final cmd = compensated_fire - 2*punch
9. Measure residual degrees between got and fire_base forward
10. if silent and cmd changed: return false from CreateMove so its caller
   does not copy compensated angles into the render camera  ; §6.7
```

Call this **first-order + iterative compensation**, not proven exact-perfect.
A runtime smoke test should log residual degrees (F1 already prints
`compensation residual_deg`). Residual ≪ 0.01° on a rifle at rest is the
acceptance bar for “good enough for this build”; it is still not a claim that
the server accepted the shot.

### 4.7 Seed width: CS fire path vs TE path

All local weapon fire callers of `FX_FireBullets` on this x64 sample follow the
same pattern (e.g. `FUN_180236AB0`, `FUN_1802373A0`, …):

```asm
MOV   EBX, dword ptr [seed_global]   ; cmd.random_seed published by prediction
MOVZX ESI, BL                        ; seed8
; push/pass ESI as FX seed argument
```

x86 fire (`FUN_1020B490`) does the same: `MOVZX EAX, BL` before
`CALL FX_FireBullets`.

The temp-entity post-data path is different:

```text
FUN_1801F0500 (CTEFireBullets PostDataUpdate):
  FX_FireBullets(..., *(this+0x3C), *(this+0x40), *(this+0x44), ...)
  ; this+0x3C is m_iSeed on the temp-entity object basis
  ; RecvProp lists m_iSeed at +0x44 on the networked-data base (see §2.2)
  ; no MOVZX — full int seed argument
```

The current CS client/server fire function receives the low-byte seed and
adds one before its polar samples: `RandomSeed(seed8 + 1)`. The temporary
entity replay passes a full networked seed to the same style of effect path,
so it is not the command-compensation input.

| Path | Seed argument | Role for perfect nospread |
| --- | --- | --- |
| Current CS client/server fire path | `random_seed & 0xFF`, then `+1` internally | **Current compensation target** |
| Local weapon wrapper | passes `random_seed & 0xFF` to the fire path | Supplies the command seed |
| `CTEFireBullets` replay | networked `m_iSeed` (int) | Other clients’ **FX only**; not cmd compensation |

Tutorial policy: implement compensation against the **local weapon** seed rule.
Do not assume TE `m_iSeed` equals the full 31-bit `cmd.random_seed`; if a
smoke test needs remote tracers to match, dump both values on a fire tick.

### 4.8 Multi-pellet policy (`Bullets > 1`)

`CCSWeaponInfo` field `Bullets` (script key `"Bullets"`, x64 info `+0x8C4`)
is the pellet count. Shotguns use values greater than 1; rifles/pistols use 1.

The current CS path takes one inaccuracy polar sample per shot and one spread
polar sample per pellet:

```text
RandomSeed(seed8+1)
inaccuracy: one angle/radius pair
pellet i: one angle/radius pair at the spread radius
```

One `CUserCmd` has **one** aim direction. You cannot put every pellet on the
same ideal point unless all pellet offsets are identical (they are not).

**Policy for this tutorial (fixed, document in F1):**

| Policy | Behavior | When |
| --- | --- | --- |
| **`pellet0` (default)** | Compensate using offsets for pellet index 0 only | Always; required for rifles |
| `none` | Skip compensation if `Bullets != 1` | Optional strict mode |
| `center` (future) | Compensate using average of pellet offsets | Optional experiment; not “perfect” |

Log `Bullets` and the chosen policy in diagnostics. Never claim “perfect for
all pellets” when `Bullets > 1`.

### 4.9 RNG: local CS polar replay (non-mutating diagnostics)

Game fire still uses process-global vstdlib `RandomSeed` / `RandomFloat`.
The tutorial **prediction** path uses a **local** `CUniformRandomStream`-style
generator and never reseeds the shared stream from the hook. Ghidra inspection
of this build's `vstdlib.dll` exports verifies the same seed normalization,
Park–Miller step, 32-entry shuffle table, float scale, and upper clamp used by
the local replay.

Proof status:

1. Two local predictions with the same inputs match bit-for-bit in the offline
   tests.
2. The active `vstdlib.dll` implementation matches the local generator.
3. F1 arms a one-shot x64 diagnostic detour of `FX_FireBullets`; the next
   matching local fire call reports the actual seed, radii, and fire angles
   beside the `CreateMove` capture.

Failure modes to fail closed on:

- local stream algorithm mismatch vs this build’s vstdlib
- predicting with full `random_seed` instead of `seed8`
- wrong `seed8+1`, polar draw order, sin/cos order, or `AngleVectors` basis
- residual degrees above the feature threshold after iteration

Callers of `FX_FireBullets` include the weapon fire functions above and TE
replay `FUN_1801F0500`. The TE path has a different seed width and object
basis, so it is useful for remote-effect validation but not a replacement for
the local command predictor.

### 4.10 Current x64 CS fire path (static correction)

The first version of this tutorial stopped at the generic
`CShotManipulator`-shaped functions. That was the source of the misleading
“everything looks straight locally” result: the predictor was replaying a
valid-looking but different RNG/vector path. The second reversal followed the
actual CS weapon call from the getter slots into the current client and server
`FX_FireBullets` implementations.

| Binary | Function | Module-relative address | Evidence |
| --- | --- | ---: | --- |
| x64 client | `FX_FireBullets` | `client.dll+0x1FFA30` | `RandomSeed(seed+1)`, `2π` angle/radius draws, separate inaccuracy/spread inputs |
| x64 client | weapon wrapper | `client.dll+0x236AB0` | calls vtable slots 382/383, passes low-byte prediction seed and both getter results |
| x64 server | CS `FX_FireBullets` | `server.dll+0x3252E0` | matching `seed+1`, separate radii, polar pair helper, per-pellet loop |
| x64 client helper | sin/cos pair helper | `client.dll+0x38F140` | returns the two float angle pair used by the cone code |
| x64 server helper | sin/cos pair helper | `server.dll+0x41CD70` | same pair-building role in the server copy |

The relevant listing shape is:

```text
seed8 = prediction_seed & 0xFF
RandomSeed(seed8 + 1)

θ0 = RandomFloat(0, 2π)
r0 = RandomFloat(0, inaccuracy)
(x0, y0) = (cos(θ0) * r0, sin(θ0) * r0)

for pellet 0 .. bullets-1:
    θ = RandomFloat(0, 2π)
    r = RandomFloat(0, spread)
    (x, y) = (x0 + cos(θ) * r, y0 + sin(θ) * r)
    FireBullet(..., x, y)
```

The exact x64 server function is the stronger authority-side evidence for
this pass. It begins with the seed increment, loads the `2π` constant, calls
the random-angle and random-radius imports, calls the pair helper, and then
repeats the spread-radius pair inside the pellet loop. The older
`server.dll+0x137890` and `client.dll+0x4FF30` functions are generic
`FireBullets`/shared helpers with a different `CShotManipulator`-style shape;
they must not be substituted for the current CS weapon path merely because
their names look related.

The live branch-2 getter pair is read directly. The wrapper updates `+0xCA8`
before the getters, but branch 2 does not consume that field. The weapon
frame path has already decayed the base-path `+0xCB0` penalty before the
wrapper reaches these getters:

```text
fire_inaccuracy = GetInaccuracy()
spread = GetSpread()
```

There is no `GetInaccuracy()+GetSpread()` equal-component inference in the
current implementation. This is the key correction for the burst snapshot in
which a valid weapon, seed, and residual were reported but bullets still went
wide.

Using the getter value captured in `ClientMode::CreateMove` was still one
simulation stage too early. Section 4.11 records the exact state transitions
between that hook and these getter calls.

### 4.11 State changes between `CreateMove` and weapon fire

The fire-time detour resolved the remaining mismatch without guessing. For
one captured AK command, the seed and spread radius matched exactly, while:

```text
CreateMove GetInaccuracy: 0.0291354
fire-time inaccuracy:     0.0277709
expected fire angle:      (-3.49101, -85.3237)
actual fire angle:        (-3.11862, -85.4470)
```

Ghidra shows two real updates between those observations.

#### Accuracy penalty decay

`client.dll+0x235FC0` is the weapon frame path. Its first virtual call is
vtable byte offset `+0xC00`, slot 384, and the attack checks occur afterward.
For the live AK vtable, slot 384 points to `client.dll+0x2367D0`.

That function reads `weapon+0xCB0`, selects a movement-state baseline and
recovery time from `CCSWeaponInfo`, and performs:

```text
factor = exp(decay_constant / recovery_time * interval_per_tick)
next_penalty = factor * (current_penalty - baseline) + baseline
```

If the current penalty is below the selected baseline, it is raised directly
to the baseline. The listing establishes these inputs for this build:

| Input | Evidence |
| --- | --- |
| move type | `player+0x1F4`; value 9 selects the ladder path |
| flags | `player+0x440`; bits 0/1 select airborne, standing, or ducking recovery |
| mode | `weapon+0xCAC`; indexes paired weapon-info floats |
| current penalty | `weapon+0xCB0` |
| extra baseline state | `weapon+0xBF4`; adds weapon-info `+0x924` when set |
| crouch / stand / ladder baseline | weapon-info `+0x8E4`, `+0x8EC`, `+0x904` plus `mode*4` |
| standing / crouch-or-airborne recovery | weapon-info `+0x91C`, `+0x920` |
| tick interval | `CGlobalVarsBase+0x1C` |

The airborne constant is `-0.7675284`; the grounded constants are
`-2.3025851`. `FUN_1803AFD60`, called with the scaled value in `XMM0`, is the
binary's float exponential implementation.

The `GlobalVars` config signature anchors this exact sequence at
`client.dll+0x2368E4`. Its RIP-relative `MOV RAX,[slot]` resolves
`client.dll+0x75E280`; one pointer read yields `CGlobalVarsBase`. The signature
was unique in the current Ghidra program.

After `FX_FireBullets`, the weapon wrapper applies the separate shot penalty.
That later update is why the tutorial keeps "pre-fire decay" and "next shot
penalty" as different diagnostic values.

The fire-time getter is not only `m_fAccuracyPenalty`. Ghidra's branch-2 tail
at `client.dll+0x235B10` calls the absolute-velocity updater at `+0x85700`,
reads horizontal velocity from `player+0x1A8/+0x1AC`, and adds a movement
component:

```text
speed = length2D(player absolute velocity)
lower = weapon_speed * 0.34
upper = weapon_speed * 0.95
movement = clamp((speed - lower) / (upper - lower), 0, 1)
           * weapon_info[0x914 + mode*4]
GetInaccuracy = movement + weapon[0xCB0]
```

The constants are the floats at `client.dll+0x47CB54` (`0.34`) and
`client.dll+0x418F1C` (`0.95`). The speed method at weapon vtable byte offset
`+0xB90` supplies `weapon_speed`; zero falls back to weapon-info `+0x728`.
The live capture therefore prints penalty and non-penalty components
separately at `CreateMove` and at fire time. A paired stationary/moving capture
isolated the remaining mismatch:

| Capture | CreateMove base | Fire-time base | Predicted penalty | Fire-time penalty |
| --- | ---: | ---: | ---: | ---: |
| stationary | `0` | `0` | `0.0280808` | `0.0289072` |
| moving at about 221 units/s | `0.09222` | `0.09222` | `0.0280808` | `0.0289072` |

The movement contribution is therefore exact in both observations. The seed,
spread radius, predicted punch, and reconstructed fire angle also match. The
same `0.00082645` penalty error remains in both captures, so changing-movement
prediction is not the cause of this result.

The optional `UpdateAccuracyPenalty` detour then removed both timing
possibilities. One capture reported the same penalty at CreateMove and
slot-384 entry, `interval_before=interval_after=0.015`, and the exact
slot-384 output:

```text
penalty_before=0.0296666
penalty_after=0.0282658
predicted=0.0274732
```

The equation was correct; its weapon-info record was not. Ghidra shows the
helper at `client.dll+0x4D700` reading a ushort from `weapon+0xC62` and tail
jumping to the table lookup at `client.dll+0x1DC630`. The implementation had
instead passed virtual weapon-id slot 371 to the lookup. Slot 371 remains the
id supplied to the fire call, but it is not evidence for the accuracy-info
index. The x64 predictor now mirrors `+0x4D700` and uses `weapon+0xC62`.

The same listing also fixes the names of the neighboring recovery fields:
grounded standing and ladder use `+0x91C`; crouching and airborne use
`+0x920`.

The next runtime capture validated the correction. For fire-call weapon id
27, the real accuracy lookup index was 46. The predictor read baseline
`0.00916`, recovery `0.48815`, and interval `0.015`, then matched slot 384 and
the fire call exactly:

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

This validates the client-side seed, one-tick penalty decay, punch decay,
fire-angle basis, radii, and polar replay for that shot. It does not by itself
prove the authoritative server shot or the physical bullet impact; a
full-magazine wall test with both command no-recoil and perfect nospread
enabled is the next fixed-point gate.

A wall test with only perfect nospread enabled does not produce a fixed cluster
because natural punch still moves the fire angle. In the captured shot,
`2*predicted_punch` contributed about 9.3 degrees while the cone offset was
about 0.13 degrees. The resulting wide recoil path cannot be judged against
the final static crosshair. Either compare each impact against that shot's
logged recoil-adjusted fire angle, or enable command no-recoil as well when
the expected observation is one compact wall cluster. Visual no-recoil only
changes rendering. Command no-recoil always suppresses the caller's camera
copy when it changes the cmd; `silent_angles` controls that choice only for
angle mutations without command no-recoil.

#### CSS punch decay

The `CCSGameMovement` RTTI locator is at `client.dll+0x54B090`; its type
descriptor resolves to the `.?AVCCSGameMovement@@` record at `+0x5E34C8`.
The locator precedes the actual vtable, which starts at `client.dll+0x4781C8`.
Vtable slot 15 (`+0x78`, stored at `client.dll+0x478240`) points to
`client.dll+0x1F5AE0`. This is the live CSS punch decay, not the base
damped-spring implementation at `client.dll+0x119390`. It reads the player
from movement-object `+0xEE0`, then uses `player+0x127C` and the same
`CGlobalVarsBase+0x1C` tick interval:

```text
length = sqrt(dot(punch, punch) + 1e-10)
next_length = max(length - (0.5 * length + 10) * interval_per_tick, 0)
next_punch = punch / length * next_length
```

The command pipeline now uses this predicted fire-time punch for both recoil
composition and subtracting the game's later `+2*punch` addition. F1 retains
the original punch and prints predicted versus actual fire-time punch so this
timing remains testable rather than assumed.

## 5. Client vs server ownership (important for the tutorial)

| Stage | Where it lives | Authoritative for multiplayer hits? |
| --- | --- | --- |
| `CUserCmd::viewangles` / buttons | Client → server | Server simulates the command |
| `random_seed` | Client fills; server should derive the same way from sequence | Shared **if** both sides use the same seed rule |
| `GetInaccuracy` / `GetSpread` | Client (and mirrored server game code) | Inputs to the current CS fire path |
| CS `FX_FireBullets` polar cone | Client and server CS fire implementation | **Authority-shaped direction** when seed and radii state match |
| Generic `CShotManipulator` / `m_vecSpread` helper | Separate shared `FireBullets` path | Not the current CS predictor target |
| `CTEFireBullets` | Network temp entity | Other clients’ visuals |

A local hook that only patches the client-side `FX_FireBullets` can still
produce **perfect-looking tracers** while a server with different state spreads
the real shot. Angle compensation on `CUserCmd` now targets the current CS
polar math, but it still depends on matching seed timing, getter values,
vector mapping, lag compensation, and server code.

Document observations honestly: “tracers straightened” ≠ “server accepted
perfect aim.”

## 6. Target design: perfect nospread + silent visual

### 6.1 Two angle channels

The client keeps more than one “where am I looking?” value:

| Channel | Typical owner | Used for |
| --- | --- | --- |
| **Sim / command angles** | `CUserCmd::viewangles` | Prediction, weapon fire direction, what the server simulates for that tick |
| **Render / camera angles** | `IVEngineClient` view (`GetViewAngles` / `SetViewAngles`, slots 19 / 20 on this build) plus punch used in view setup | What you see on screen |

The shared pipeline **must** write the final fire-space compensation back into
the **command** (after reversing the game's `+2*punch` addition). It must
**not** leave that
per-shot jitter in the **camera** if the goal is “no visible shaking.”

```text
intended look  (what the player / aimbot chose)
      |
      v
 +--------------------+     seed + GetInaccuracy + GetSpread
  | config-aware shot  | --> recoil fire basis + CS-polar inverse cone
 | pipeline           |
 +--------------------+
      |
   |  cmd.viewangles = compensated CS-polar angle - 2*punch  <--- simulation channel
      |
       +--> return false when command no-recoil is applied, or when
            silent angles suppress another command-angle mutation
      |
      v
prediction / fire uses cmd  -->  post-spread bullet == intended
caller skips SetViewAngles   -->  camera keeps intended look
```

This is the classic **silent** split: *the shot is corrected; the view is not.*

### 6.2 Why compensation shakes without restore

Each attack tick (especially sprays) can require a **different** inverse cone
because `random_seed` and `m_fAccuracyPenalty` change. Writing those angles
only into `CUserCmd` is correct for sim, but if the engine also adopts the
command angles as the local eye angles for rendering, the camera **flicks**
by the cone-inverse every shot. That is not punch recoil; it is compensation
noise.

There is an important lifecycle trap here. Ghidra confirms it in the current
x64 `client.dll`, not only in the Source SDK. `FUN_180152290` builds the real
ring-buffer command. At `client.dll+0x1524BD` it copies the angle returned by
`VEngineClient014::GetViewAngles` into `cmd+0x10` while alive. It calls
`ClientMode::CreateMove` at `+0x15252F`, tests the boolean return, and only on
`true` calls `VEngineClient014::SetViewAngles(cmd+0x10)` at `+0x15254B`.
`FUN_1801527B0` repeats the same return-controlled copy for the temporary
extra-mouse-sample command at `+0x1528FA..+0x152912`.

A `SetViewAngles` call made inside our detour can therefore be overwritten by
the caller. The stable simulation/camera split is to return `false` when the
compensated command must stay out of the camera. The command still proceeds to
seed, verification, and networking.

### 6.3 Layering with existing features

Order inside `CreateMove` (after `originalCreateMove` returns, matching the
current hook shape) is one shared, config-aware shot pipeline:

```text
1. Save inputAngles = cmd.viewangles.
2. Aimbot may replace the desired aim direction.
3. Read current punch and apply the verified one-tick CSS decay to obtain the
   punch that the fire wrapper will consume.
4. Apply the verified one-tick weapon penalty decay, then invert the predicted
   cone around that fire-space direction when perfect nospread is enabled.
5. Convert the compensated fire direction back through the game's +2*punch
   fire path and write one final cmd.viewangles.
6. If command no-recoil changed the cmd, return `false`. For mutations without
   command no-recoil, use `silent_angles` to select whether the caller copies
   the cmd into the camera.
```

The toggles change the math, not just whether independent writers run. The
pure `game::ComposeShotAngles` helper is unit-tested without a live process:

| Configuration | Fire-space base | Final command |
| --- | --- | --- |
| No recoil off, no spread off | desired aim + natural `2*punch` | desired aim |
| No recoil on | desired aim | desired aim minus current `2*punch` |
| No recoil off, no spread on | desired aim + natural `2*punch` | inverse-spread result minus `2*punch` |
| Both on | desired aim | inverse-spread result minus current `2*punch` |

In other words, when no-recoil is disabled, no-spread must compensate around
the naturally punched fire direction or it would accidentally remove recoil.
When no-spread data is unavailable, the pipeline fails closed for that stage
and retains aim plus any readable recoil stage. Command no-recoil has one
fire-space equation:

```text
command = desired - current_2punch
```

Ghidra also shows why a non-silent recurrence is not an exact alternative.
The current fire path consumes `cmd + 2*punch`, while the x64 render-view path
`FUN_180054450` adds the three punch fields at entity offsets
`+0x127C/+0x1280/+0x1284` only once. Copying an exact fire-compensated cmd into
the camera therefore produces an opposite camera displacement: the fire and
render paths use different punch multipliers. Repeated absolute subtraction
also feeds the corrected camera into the next command and drives pitch into
the `89`-degree clamp; runtime exposed that failure as `desired.pitch=89` with
`residual_deg=9.22407`.

The implementation consequently returns `false` whenever command no-recoil
actually changes the cmd, regardless of `silent_angles`. There is no
incremental workaround: it would trade exact fire correction for a different
camera error instead of matching both verified paths.

The first visual no-recoil implementation zeroed `m_vecPunchAngle` around
`FrameStageNotify(FRAME_RENDER_START)`. Runtime testing showed that enabling
it made otherwise exact command no-recoil plus nospread miss. Ghidra explains
why: x64 `FrameStageNotify` at `client.dll+0xD59B0` dispatches stage 5 to
`client.dll+0xD6D70`, which contains `Client SimulateEntities`, temporary
entity simulation, and particle simulation. Stage 5 is not a render-only
window. Writing punch there can change state consumed by simulation, and
restoring an older value after the original call can overwrite a legitimate
update.

The replacement is read-only with respect to the player. Ghidra verifies the
following path:

- x64 ClientMode vtable `client.dll+0x477638`, slot 16
  `client.dll+0xE67C0`;
- x86 ClientMode vtable `client.dll+0x397E04`, slot 16
  `client.dll+0xF6220`;
- both `OverrideView` implementations access `CViewSetup::origin` at `+0x40`
  and `CViewSetup::angles` at `+0x4C`;
- x64 `CViewRender::SetUpView` at `client.dll+0x1BB5E0` asks the player to
  calculate the view, then calls ClientMode slot 16, then builds render
  matrices;
- the player view function at `client.dll+0x54450` adds one current punch to
  the camera angles before that ClientMode call.

The hook therefore calls the original `OverrideView` first and subtracts one
current punch from `CViewSetup::angles`. It never writes
`m_vecPunchAngle`. This visual layer still does **not** cancel nospread
compensation flicker; that remains the CreateMove return-value split.

| Visual problem | Source | Cancellation |
| --- | --- | --- |
| Punch kick on screen | One punch added during player view calculation | Subtract one punch from `CViewSetup::angles` after `OverrideView` |
| Compensation / silent aim snap | `cmd.viewangles` adopted as eye angles | Return `false` so the caller skips its camera copy |
| Actual bullet cone | `RandomFloat` in fire path | Perfect nospread on **cmd only** |

Do not confuse “zero punch for render” with “perfect nospread.” They compose.

### 6.4 Optional third cherry: local FX cone

Even with perfect sim compensation, **other players’** or local **tracers**
built from `FX_FireBullets` still sample the cone unless the radii or seed
path is changed. For a local-only cosmetic polish you can later:

- leave sim compensation as the authority path, and
- optionally force local FX radii to 0 **only as a visual**, knowing that is
  independent of hit registration.

That FX tweak is **not** a substitute for perfect nospread; it is an optional
tracer cosmetic on top of silent angle restore.

### 6.5 Config shape (implemented)

INI keys under `[features]` in `config/signatures.ini` /
`config/signatures-x64.ini` (loaded into runtime `features::Config`):

| INI key | Runtime field | Default | Meaning |
| --- | --- | --- | --- |
| `perfect_nospread` | `perfectNoSpread` | `false` | Fire-space first-order+iterative cmd compensation |
| `norecoil` | `norecoil` | `true` | Include punch cancellation in the shared fire-angle pipeline |
| `silent_angles` | `silentAngles` | `true` | Suppress camera copies for aim/no-spread mutations; command no-recoil always suppresses its own camera copy |
| `visual_norecoil` | `visualNoRecoil` | `true` | Read-only camera correction in ClientMode slot 16 |
| (fixed policy) | pellet0 | — | Multi-pellet policy (§4.8); not a separate INI yet |

Keep `perfect_nospread=false` until F1 residuals look plausible.

### 6.6 `CreateMove` / `SetViewAngles` notes for this repo

Ghidra evidence on x64 `engine.dll` for `VEngineClient014` vtable at
`engine.dll+0x366070` (object factory path documented in chapter 004):

| Slot | Address (sample) | Behavior |
| ---: | --- | --- |
| 19 `GetViewAngles` | `engine.dll+0x70630` | Copies three floats from `engine.dll+0x53E4E4..EC` into the caller’s `QAngle` |
| 20 `SetViewAngles` | `engine.dll+0x70F20` | Writes those three globals after per-component normalize (`FUN_1802762F0`: wrap to ±180) |

```asm
; GetViewAngles (slot 19) — this = RCX, out QAngle* = RDX
MOVSS XMM0, dword ptr [engine+0x53E4E4]
MOVSS dword ptr [RDX], XMM0
MOVSS XMM1, dword ptr [engine+0x53E4E8]
MOVSS dword ptr [RDX+4], XMM1
MOVSS XMM0, dword ptr [engine+0x53E4EC]
MOVSS dword ptr [RDX+8], XMM0
RET

; SetViewAngles (slot 20) — this = RCX, in QAngle* = RDX
; stores AngleNormalize(pitch/yaw/roll) into the same three globals
```

Implementation rules:

- Keep `game::SetViewAngles` as a diagnostic-verified interface, but do not
  use it as the silent restore mechanism from inside `CreateMove`.
- Compose aim, recoil, and spread once in `game::ComposeShotAngles`; do not
  let separate feature functions overwrite each other's command angles.
- Perfect nospread only on fire-relevant cmds (`IN_ATTACK` + usable weapon).
- If no-spread resolution fails, preserve the selected aim and apply only a
  readable recoil stage; never write a partial inverse-cone result.
- If command no-recoil changes the cmd, return `false` even when
  `silent_angles=false`; exact fire compensation and a copied render angle are
  incompatible because the verified punch multipliers are two and one.
- Silent camera suppression does **not** replace `visualNoRecoil`.
- F1 runs **after** the shared pipeline so the dump shows desired, fire-base,
  final command, applied stages, cone, and residual for that tick.

### 6.7 Silent camera scope and lifecycle (decision)

**Decision: return `false` whenever command no-recoil changed
`cmd->viewangles`. For other angle mutations, return `false` only when
`silent_angles` is enabled.**

Rationale:

- Exact no-recoil requires `cmd = desired - 2*punch`, while rendering adds
  only `1*punch`; copying that cmd into the camera creates inverse recoil.
- Aimbot and no-spread remain visibly selectable through `silent_angles` when
  command no-recoil is not part of the mutation.
- The suppression must happen before the engine's post-`CreateMove` copy:

```text
saved = cmd.viewangles at hook entry (post-originalCreateMove)
... features may mutate cmd ...
if cmd.viewangles != saved and (noRecoilApplied or silentAngles):
    return false
else:
    return originalCreateMove_result
```

Refinement when aimbot + silent are both on: `saved` should be the **player’s
true look** if aimbot is also silent; if aimbot is *visible*, save post-aimbot
angles as the intended camera. For this tutorial, start with:

```text
input_angles    = angles after originalCreateMove
desired_angles  = Aimbot(input_angles) or input_angles
cmd.viewangles  = ComposeShotAngles(
                    desired_angles,
                    current_punch,
                    config.norecoil,
                    config.perfect_nospread)
if cmd changed and (noRecoilApplied or silentAngles):
    return false from CreateMove
```

When command no-recoil composes with aimbot or no-spread, the one CreateMove
return value suppresses the entire combined command angle. Without command
no-recoil, `silent_angles=false` still permits visible aim/no-spread angles.

Perfect nospread itself still only **computes** on fire-relevant ticks
(`IN_ATTACK` and weapon can fire); other ticks skip the cone math.

### 6.8 Relation to existing tutorial features

| Feature | Hook stage | Overlap with spread / silent |
| --- | --- | --- |
| Bunny hop / triggerbot | `CreateMove` | buttons only |
| Aimbot | `CreateMove` viewangles | supplies the **desired** aim before fire-space composition |
| No-recoil (cmd) | `CreateMove` | selects the punch policy used by the shared pipeline |
| Visual no-recoil | `ClientMode::OverrideView` | read-only camera punch removal after the original call |
| Perfect nospread | `CreateMove` | inverse cone on **cmd** |
| Silent camera | `CreateMove` return value | `false` when cmd angles changed |
| F1 debug | `CreateMove` | dump stage flags, fire-base/command angles, seed, separate fire radii, residual, and engine delta |

Recommended order of work:

1. Read-only weapon + seed + separate-radii diagnostic (F1).
2. Prove the CS polar reconstruction offline (deterministic vectors and no mutation).
3. Cmd-only perfect nospread behind a default-off toggle (expect camera shake).
4. Return `false` for changed cmd angles when silent mode is on (camera shake goes away).
5. Confirm visual no-recoil still composes; only then consider local FX cosmetics.

Do **not** scan signatures or walk weapons inside `EndScene`.

## 7. Diagnostic (implemented)

Resolution lives in `game/weapon.cpp` and `game/interfaces.cpp`, not in the
feature body.

F1 (after mutation in `CreateMove`) prints:

```text
spread diag: weapon=0x... handle=0x...
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

The next matching x64 fire call additionally prints:

```text
client fire-time diag:
  player=... weaponId=... weaponInfoIndex=... mode=... command=... branch=...
  seed expected8=... actual8=... match=...
  radii create_move_getter=(...) predicted_fire_inaccuracy=... actual=(...) delta=(...) predicted_delta=(...)
  accuracy decay=... baseline=... recovery=... interval=...
  angles cmd=(...) expected_fire=(...) actual_fire=(...) delta=(...)
  update_accuracy calls=... penalty_before=... penalty_after=... interval_before=... interval_after=...
  inaccuracy components create_penalty=... create_base=... actual_penalty=... actual_base=...
  movement create=speed2d=... flags=... movetype=... actual=speed2d=... flags=... movetype=...
  punch create=(...) predicted_fire=(...) actual=(...)
  fire source=(...) reconstructed=(...) delta=(...)
  offsets create_move=(...) actual_args_replay=(...)
  final direction target=(...) residual_deg=...
```

With camera suppression active through command no-recoil or
`silent_angles=true`, a healthy fire tick shows **cmd** diverging from
**engine** while the camera stays on the intended look.
For the observed branch-2 AK, the getter taken after the verified pre-fire
decay is the expected fire radius. The fire-time block is the acceptance
evidence: `predicted_delta`, punch prediction, reconstructed fire angle, and
cone offsets must agree with the actual call before this pass is considered
validated.

## 8. Signature strategy (if hooks are needed later)

Prefer:

1. Netvars for weapon fields (`m_fAccuracyPenalty`, `m_weaponMode`,
   `m_hActiveWeapon`, `m_iClip1`).
2. Virtual calls on a live weapon object for `GetInaccuracy` / `GetSpread`
   (slot verification via vtable dump of a known weapon).
3. The configured `WeaponInfoLookup` function signature for the next-shot
   penalty formula.
4. The x64 diagnostic resolves `FX_FireBullets` through the unique call-site
   signature at `client.dll+0x236C90`; its rel32 call target is
   `client.dll+0x1FFA30`.
5. `GlobalVars` resolves the pointer slot from the unique
   `UpdateAccuracyPenalty` sequence at `client.dll+0x2368E4`.
6. `UpdateAccuracyPenalty` resolves the unique function entry at
   `client.dll+0x2367D0` for the optional entry/exit diagnostic detour.

Do **not** hardcode Ghidra absolute addresses into the DLL. Log
module-relative matches the same way as ClientMode / ClientState.

## 9. Runtime validation checklist

- [x] x64 process + x64 `client.dll`
- [x] `m_hActiveWeapon` netvar resolves; handle → non-null weapon
- [x] weapon vtable readable; slots `0xBF0` / `0xBF8` / `0xB98` executable
- [x] `WeaponInfoLookup` has one match and returns readable data for the live id
- [x] `GetInaccuracy` / `GetSpread` return finite floats in a plausible range
      (near 0 when standing still with a rifle after recover; larger when
      moving / mid-spray)
- [x] live branch is reported; branch 2 uses the getter predicted after its
      pre-fire penalty decay, while branch 1 remains fail-closed until its
      special path is implemented
- [x] fire-time actual seed, predicted pre-fire radii, and angles match the
      captured command
- [x] slot-384 diagnostic reports its exact penalty and interval before and
      after the update; use these values before revising the decay predictor
- [x] predicted punch equals the actual `player+0x127C` value at fire time
- [x] `fire source + 2*actual punch` reconstructs the hooked fire angle
- [x] CS polar replay uses `seed8+1`, and its deterministic offline vectors
      match the local ran1-style stream and angle/radius draw order
- [x] `m_fAccuracyPenalty` rises after firing and decays over time
- [x] Positive `command_number` values change while firing and
      `effective_seed` matches the seed the game publishes after
      `ClientMode::CreateMove` (diagnostic only)
- [x] Zero-sequence extra samples are reported as unavailable and do not receive
      command-zero compensation
- [ ] No mutation while these checks fail

## 10. x86 cross-check (Ghidra program `/client.dll`)

Same CS:S client, PE32 (`x86:LE:32:default`, image base `0x10000000`).

### 10.1 Netvars / fields (RecvProp registration)

| Property | Table | x86 offset | x64 offset |
| --- | --- | ---: | ---: |
| `m_hActiveWeapon` | `DT_BaseCombatCharacter` | `0xD80` | `0x11A0` |
| `m_iShotsFired` | `DT_CSLocalPlayerExclusive` | `0x1540` | `0x1A34` |
| `m_weaponMode` | `DT_WeaponCSBase` | `0x92C` | `0xCAC` |
| `m_fAccuracyPenalty` | `DT_WeaponCSBase` | `0x930` | `0xCB0` |
| `m_iClip1` | `DT_LocalWeaponData` | `0x8BC` | `0xC30` |

Offsets **differ by architecture** (pointer padding). Prefer runtime
`netvars::GetOffset` for both; use this table only as a static cross-check.

### 10.2 Vtable slots (stable) vs byte offsets (not stable)

| Method | Slot (0-based) | x86 byte off | x64 byte off |
| --- | ---: | ---: | ---: |
| `GetInaccuracy` | **382** | `0x5F8` | `0xBF0` |
| `GetSpread` | **383** | `0x5FC` | `0xBF8` |

Evidence: x86 fire `FUN_1020B490` calls `[EAX+0x5F8]` / `[EAX+0x5FC]`; x64
fire uses `[RAX+0xBF0]` / `[RAX+0xBF8]`. Same slot index, different pointer
width.

### 10.3 Active weapon resolve

| Arch | Function | Handle off | List stride |
| --- | --- | ---: | ---: |
| x86 | `FUN_10073BA0` | `+0xD80` | `0x10` |
| x64 | `FUN_18004A890` | `+0x11A0` | `0x20` |

Both use `index = handle & 0xFFFF`, serial `handle >> 16`, null on mismatch.

#### 10.3.1 x64 `CBaseHandle` parameter ABI

The first implementation declared the handle-taking entity-list virtuals as
accepting `uint32_t`. That was wrong for MSVC x64 even though the stored
`m_hActiveWeapon` value is four bytes. The matching Source SDK declares
`GetClientEntityFromHandle(CBaseHandle)` by value, where `CBaseHandle` is a
non-trivial four-byte class.

Ghidra confirms the distinction in the current x64 binary:

| Evidence | Address |
| --- | ---: |
| `VClientEntityList003` string | `client.dll+0x42D770` |
| interface object / vtable path | `client.dll+0x6498C8` / `+0x42D7B0` |
| `GetClientEntityFromHandle`, slot 4 | `client.dll+0xDF700` |

The slot-4 listing begins:

```asm
MOV EAX, dword ptr [RDX]
LEA RDX, [RSP + 0x30]
MOV dword ptr [RSP + 0x30], EAX
CALL qword ptr [RAX + 0x10]
```

`RDX` therefore points at a `CBaseHandle` temporary. The SDK-shaped
declaration in `sdk/client_entity_list.h` preserves that ABI, while the
runtime diagnostic still prints the raw handle, entry index, and serial. A
log such as `handle=0x01390146 index=0x146` proves only that the field read
worked; the resolution is not successful until `weapon=0x...` and
`methodsOk=yes` are reported.

### 10.4 Seed and cone

x86 weapon fire also `MOVZX` the seed global low byte before its
`FX_FireBullets` call. Re-verify the x86 helper and server copy independently;
the x64 evidence should not be copied blindly. The current implementation
keeps the same API shape—separate inaccuracy and spread inputs—but x86
compensation is only considered validated after its own polar instruction
order, seed increment, and pair orientation have been checked.

## 11. Decisions closed in this chapter

| Topic | Decision |
| --- | --- |
| Multi-pellet | Default **`pellet0`**; no “perfect for all pellets” claim |
| Local seed | **`MD5_PseudoRandom(command_number) & 0x7fffffff`, then low byte** |
| TE seed | Full `m_iSeed` for remote FX only; Recv `+0x44` vs object `+0x3C` bases |
| Silent camera | Return `false` when command no-recoil changes the angle; otherwise require `silent_angles` for a changed command (§6.7) |
| Visual punch | Subtract one punch from `CViewSetup::angles` after ClientMode slot 16; never write player punch during frame stages |
| RNG for predict | **Local** UniformRandomStream replay of the CS polar sequence; never reseed vstdlib from F1 |
| Compensation | First-order + iterative; log residual degrees; not claimed exact-perfect |
| CS right/up | `sx = cosθ·r`, `sy = sinθ·r`; add one inaccuracy sample to pellet spread sample |
| Fire radii mapping | Observed branch 2: one slot-384 penalty decay, then `fire_inaccuracy = GetInaccuracy()` and `spread = GetSpread()`; branch 1 remains unimplemented |
| Fire punch | Apply the CSS movement slot-15 one-tick decay before reversing the wrapper's `+2*punch` addition |
| Fail-closed | Require weapon-info formula + both methods + separate radii + clip read success + clip>0 + residual ≤ 1° |

## 12. Implementation status

| Item | Status |
| --- | --- |
| `game::SetViewAngles` (slot 20) | Diagnostic-only; not used for silent camera |
| Weapon resolve + GetInaccuracy/GetSpread | Implemented + bounds; x64 handle ABI corrected |
| Weapon id + accuracy branch diagnostic | Implemented; live AK reports branch 2 |
| Pre-fire accuracy decay | Validated against slot 384 / `client.dll+0x2367D0`; predicted and actual penalty/radius match exactly for the captured shot |
| Pre-fire CSS punch decay | Validated against movement slot 15 / `client.dll+0x1F5AE0`; predicted and actual fire-time punch match |
| mode/clip/penalty read-success flags | Implemented |
| CS polar cone predict (non-mutating) | Implemented; seed8+1, separate radii, local stream |
| Generic `CShotManipulator` path | Documented as a separate path; not used for compensation |
| First-order + iterative compensate | Implemented |
| Config-aware aim/recoil/fire-space composition | Exact `desired-2*punch`; camera copy is always suppressed when command no-recoil mutates the angle; offline tests cover spread on/off |
| F1 fire-time argument capture | Implemented on x64 through the verified `FX_FireBullets` call target |
| `perfect_nospread` (default **off**) | Implemented |
| `silent_angles` camera suppression | Implemented via the CreateMove return value |
| `visual_norecoil` camera correction | Implemented through ClientMode slot 16; player punch remains read-only |
| Offline deterministic math/ABI tests | `tests/weapon_math_tests.cpp`; CTest passes on x86 and x64 |

The current x64 sample has live captures with matching command/fire seed,
pre-fire radii, punch, fire angle, cone replay, and zero final-direction
residual. Local bot testing reported accurate hits with command no-recoil and
perfect no-spread, and the visual correction was accepted as good enough for
this pass after it moved to read-only `OverrideView`. This remains
**exploratory**: it does not prove another build, the unimplemented branch-1
accuracy path, the x86 cone path, or an authoritative remote server.

The offline test target covers command-seed vectors, zero-sequence handling,
`CUserCmd` layout, cone determinism, and compensation residuals. Builds and
offline tests for x86/x64 pass.

## 13. Reversal record (summary)

```text
Question/behavior: perfect nospread (CS polar seed/radii inverse cone on cmd) + silent camera
Binaries: client.dll x64 + x86; server.dll x64; engine.dll x64 for view
Ghidra: /source-engine-tutorial/x64/client.dll and server.dll.1 (0x180000000), /client.dll (0x10000000)
CS cone: seed8+1 inside client.dll+0x1FFA30 / server.dll+0x3252E0; polar inaccuracy/spread sequence; pellet0
Fire radii mapping: live branch-2 applies slot-384 pre-fire +0xCB0 decay, then keeps `GetInaccuracy()` and `GetSpread()` separate; the shot penalty follows fire
Punch timing: CSS movement vtable slot 15 at client.dll+0x1F5AE0 applies one tick of length decay before the wrapper reads player+0x127C
Generic trace: client.dll+0x4FF30 and server.dll+0x137890 are separate paths, not this predictor
Seed: CS weapon path MOVZX low byte then fire-path +1; TE m_iSeed full int (object +0x3C / Recv +0x44)
Compensation: first-order + iterative residual measure
Pipeline: desired aim -> config-selected recoil fire basis -> inverse spread ->
command angle minus the game's +2*punch fire addition
Silent: always suppress a command no-recoil camera copy; use `silent_angles`
for other angle mutations; visual punch removal is read-only in OverrideView
Config INI: perfect_nospread=false; silent_angles=true
Static ABI verification: x64 CBaseHandle slot-4 listing, the
`FUN_180152290`/`FUN_1801527B0` input ordering, the client/server CS polar
fire loop, and the weapon-info accuracy update ordering are confirmed;
runtime x64 fire-state timing matched the captured command, while x86 cone
runtime validation remains open; offline weapon-math tests pass on both x86
and x64
```

## Remaining validation boundary

The documented x64 sample satisfies the static evidence, checked-read,
diagnostic, build, offline-test, and local-smoke-test gates used in this pass.
The feature remains labeled **exploratory** because branch 1, x86 live cone
behavior, cross-build signatures, and authoritative remote-server behavior
have not been validated.
