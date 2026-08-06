# 005 — No-spread exploration: weapon accuracy, seed, and fire path

This chapter records a **read-first** reverse of how the current x64 CS:S
client applies bullet cone noise. It does **not** claim a finished no-spread
feature. The goal is to know **which subsystem owns the behavior**, which
values are networked versus client-only, and what a safe diagnostic should
print before any mutation is considered.

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
  -> FX_FireBullets(seed, inaccuracy, spread) drives RandomFloat cone
  -> optional CTEFireBullets replicates seed + cone values for other clients
```

“No-spread” could mean any of:

1. **Visual-only**: zero the cone inside `FX_FireBullets` or force
   `GetInaccuracy`/`GetSpread` to `0` on the local client.
2. **Angle compensation (“perfect no-spread”)**: leave the fire path alone,
   reverse the deterministic `RandomFloat` samples from
   `(seed, inaccuracy, spread)`, and pre-adjust `CUserCmd::viewangles` so the
   *post-spread* direction matches the intended aim.
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

```asm
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
seed: a zero-sequence command is an extra mouse sample and the feature skips
compensation for it. A valid runtime dump should show positive command numbers
while firing, `seed_source=command_number` at this hook, and a changing
`seed8`; `effective_seed=unavailable seed_source=zero_sequence` identifies the
temporary path.

### 3.2 Compile-time layout check recommendation

Add `static_assert` offsets for `random_seed` (and other fields used by
features) so x86/x64 packing cannot silently drift. Do not assume an x86
overlay without re-measuring.

## 4. Fire path: `GetInaccuracy` → `GetSpread` → `FX_FireBullets`

### 4.1 Example primary-fire function

`FUN_180236ab0` (`client.dll+0x236AB0`) is a shared gun fire path. Listing
evidence (weapon `this` in `R15`, owner player in `RBP`):

```asm
; accuracy scalar from AccuracyDivisor / shots fired → weapon+0xCA8
MOVSS  dword ptr [R15 + 0xCA8], XMM0

; virtual GetSpread
CALL   qword ptr [RAX + 0xBF8]   ; float in XMM0 → spread
; virtual GetInaccuracy
CALL   qword ptr [RAX + 0xBF0]   ; float in XMM0 → inaccuracy

MOV    EBX, dword ptr [client.dll+0x5A6000]  ; prediction seed global
MOVZX  ESI, BL

CALL   FX_FireBullets(... seed=ESI, inaccuracy, spread ...)

; after fire, add InaccuracyFire[mode] into m_fAccuracyPenalty
ADDSS  XMM1, dword ptr [weaponInfo + R12*4 + 0x90C]
MOVSS  dword ptr [R15 + 0xCB0], XMM1
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

### 4.3 Virtual methods on the weapon

| Method | Vtable byte offset | Slot (÷8) | Implementation (sample) |
| --- | ---: | ---: | --- |
| `GetInaccuracy` | `0xBF0` | 382 | `FUN_180235B10` |
| `GetSpread` | `0xBF8` | 383 | `FUN_180235D90` |

`GetInaccuracy` combines:

- movement speed vs weapon max speed thresholds,
- `InaccuracyMove[mode]` from weapon info (`+0x914` + `4*mode`),
- plus `m_fAccuracyPenalty` at `weapon+0xCB0`.

`GetSpread` returns `0` when `weapon_accuracy_model == 1`, otherwise
`Spread[mode]` from weapon info (`+0x8DC` + `4*mode`).

These slots are for **this** build’s `C_WeaponCSBase` layout. Re-verify if the
vtable grows; do not treat 382/383 as eternal constants.

### 4.4 `FX_FireBullets` cone math (exact for this binary)

Function: `FUN_1801FFA30` (`client.dll+0x1FFA30`).
Imports: `RandomSeed`, `RandomFloat` from vstdlib.
Helper: `FUN_18038F140` builds a two-float pair via `sin(theta)` and
`sin(theta + pi/2)` (= `cos(theta)`).
Constant at `client.dll+0x3FB6E8` = `0x40C90FDB` = **2π**.

Listing-backed sequence when a local shooter object exists:

```text
; weapon path: seed_argument is already (random_seed & 0xFF)
; TE path: seed_argument is the networked m_iSeed (full int; see §4.7)
RandomSeed(seed_argument + 1)

; shared inaccuracy sample (once per shot)
θ0 = RandomFloat(0.0, 2π)
r0 = RandomFloat(0.0, inaccuracy)
; basis confirmed by x86 fcos/fsin in FUN_101DDA90 and x64 FireBullet use:
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
same radii and `fcos`/`fsin` explicitly, which is the cleanest read of the
right/up assignment used above.

### 4.5 `FireBullet` direction construction

`FUN_1801F9BB0` applies the offsets after `AngleVectors` on the aim angles
(`FUN_1802AEA60`, degrees → radians with `* 0.017453292`):

```text
AngleVectors(angles, forward, right, up)
dir = forward + right * sx + up * sy
dir = normalize(dir)
; then trace along dir
```

So the **simulation channel** that perfect nospread must invert is:

```text
intended_forward
  = normalize( AngleVectors(cmd_angles).forward
               + right * sx + up * sy )
```

with `(sx, sy)` produced by the RNG sequence above for the same seed and
radii the weapon will use on that tick.

### 4.6 Inverse for perfect nospread (first-order + iteration)

The game applies spread **after** `AngleVectors(cmd.viewangles)`. A single
subtraction of `(sx, sy)` on the **intended** basis is therefore only a
**first-order** inverse: after you write compensated angles, the engine
recomputes a new `forward/right/up` and the residual is not identically zero.

Tutorial implementation (`game::CompensateAngles`):

```text
1. intended = cmd.viewangles   ; or aimbot / norecoil output
2. seed     = MD5_PseudoRandom(cmd.command_number) & 0x7fffffff
   seed8    = seed & 0xFF
3. radii    = GetInaccuracy / GetSpread
4. (sx, sy) = local UniformRandomStream(seed8+1) pellet0 sequence
5. First-order: aim = normalize(F_i - R_i*sx - U_i*sy) on intended basis
6. cmd = VectorAngles(aim)
7. Iterate a few times using the **cmd** basis:
     got = normalize(F_c + R_c*sx + U_c*sy)
     aim = normalize(F_intended - R_c*sx - U_c*sy)
     cmd = VectorAngles(aim)
8. Measure residual degrees between got and intended forward
9. if silent and cmd changed: return false from CreateMove so its caller
   does not copy compensated angles into the render camera  ; §6.7
```

Call this **first-order + iterative compensation**, not proven exact-perfect.
A runtime smoke test should log residual degrees (F1 already prints
`compensation residual_deg`). Residual ≪ 0.01° on a rifle at rest is the
acceptance bar for “good enough for this build”; it is still not a claim that
the server accepted the shot.

### 4.7 Seed width: weapon path vs TE path

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

Inside `FX_FireBullets`, both paths end as `RandomSeed(param_seed + 1)`.

| Path | Seed argument | Role for perfect nospread |
| --- | --- | --- |
| Local weapon fire | `random_seed & 0xFF` | **This is what we compensate** |
| `CTEFireBullets` replay | networked `m_iSeed` (int) | Other clients’ **FX only**; not cmd compensation |

Tutorial policy: implement compensation against the **local weapon** seed rule.
Do not assume TE `m_iSeed` equals the full 31-bit `cmd.random_seed`; if a
smoke test needs remote tracers to match, dump both values on a fire tick.

### 4.8 Multi-pellet policy (`Bullets > 1`)

`CCSWeaponInfo` field `Bullets` (script key `"Bullets"`, x64 info `+0x8C4`)
is the pellet count. Shotguns use values greater than 1; rifles/pistols use 1.

The RNG does:

```text
one shared (sx0, sy0) from inaccuracy
then for each pellet i: independent (sxi, syi) from spread
FireBullet with (sx0+sxi, sy0+syi)
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

### 4.9 RNG: local stream for prediction (non-mutating diagnostics)

Game fire still uses process-global vstdlib `RandomSeed` / `RandomFloat`.
The tutorial **prediction** path uses a **local** `CUniformRandomStream`-style
generator (ran1 / Source SDK algorithm) so F1 never reseeds the shared stream.

Proof plan:

1. F1 on a stable weapon: log `seed8`, radii, predicted `(sx,sy)`, residual
   degrees after compensation (local stream only).
2. Two local predictions with the same inputs must match bit-for-bit.
3. Optional stronger check: one-shot diagnostic detour of `FX_FireBullets` to
   compare game `(sx,sy)` with the local predictor; do not ship that detour.

Failure modes to fail closed on:

- local stream algorithm mismatch vs this build’s vstdlib
- predicting with full `random_seed` instead of `seed8`
- wrong sin/cos assignment or degree/radian mix in `AngleVectors`
- residual degrees above the feature threshold after iteration

Callers of `FX_FireBullets` include the weapon fire functions above and TE
replay `FUN_1801F0500`.

## 5. Client vs server ownership (important for the tutorial)

| Stage | Where it lives | Authoritative for multiplayer hits? |
| --- | --- | --- |
| `CUserCmd::viewangles` / buttons | Client → server | Server simulates the command |
| `random_seed` | Client fills; server should derive the same way from sequence | Shared **if** both sides use the same seed rule |
| `GetInaccuracy` / `GetSpread` | Client (and mirrored server game code) | Server weapon code for hit traces |
| `FX_FireBullets` cone | Client FX + prediction | Effects / local prediction; not a substitute for server traces |
| `CTEFireBullets` | Network temp entity | Other clients’ visuals |

A local hook that only patches `FX_FireBullets` can produce **perfect-looking
tracers** while the server still spreads the real shot. Angle compensation on
`CUserCmd` is the approach that tries to stay consistent with a shared seed,
but it still depends on matching math and is easily broken by prediction
mismatch, lag compensation, or different server weapon code.

Document observations honestly: “tracers straightened” ≠ “server accepted
perfect aim.”

## 6. Target design: perfect nospread + silent visual

### 6.1 Two angle channels

The client keeps more than one “where am I looking?” value:

| Channel | Typical owner | Used for |
| --- | --- | --- |
| **Sim / command angles** | `CUserCmd::viewangles` | Prediction, weapon fire direction, what the server simulates for that tick |
| **Render / camera angles** | `IVEngineClient` view (`GetViewAngles` / `SetViewAngles`, slots 19 / 20 on this build) plus punch used in view setup | What you see on screen |

Perfect nospread **must** write the compensated direction into the **command**
(and anything prediction uses for that fire). It must **not** leave that
per-shot jitter in the **camera** if the goal is “no visible shaking.”

```text
intended look  (what the player / aimbot chose)
      |
      v
 +--------------------+     seed + GetInaccuracy + GetSpread
 | perfect nospread   | --> inverse cone math
 | (CreateMove)       |
 +--------------------+
      |
      |  cmd.viewangles = compensated   <--- simulation channel
      |
       +--> return false from CreateMove when silent angles are enabled
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

There is an important lifecycle trap here. The matching Source SDK client
path calls `g_pClientMode->CreateMove(...)` and then, only when it returns true,
calls both `engine->SetViewAngles(cmd->viewangles)` and
`prediction->SetLocalViewAngles(cmd->viewangles)`. A `SetViewAngles` call made
inside our detour is therefore overwritten by the caller. A later
`FRAME_RENDER_START` restore is also vulnerable to extra input samples and
other camera writes. The stable silent path for this hook is to return false
when our code changed `cmd->viewangles`; the command still proceeds to seed,
verification, and networking, while the caller leaves the render camera
alone. This is the fix for the steadily spinning view observed while firing.

### 6.3 Layering with existing features

Order inside `CreateMove` (after `originalCreateMove` returns, matching the
current hook shape) should be explicit:

```text
1. Save intendedAngles = cmd.viewangles  (or engine GetViewAngles)
2. Aimbot may replace intended aim on cmd
3. No-recoil may adjust cmd for punch  (sim channel)
4. Perfect nospread adjusts cmd for inverse cone  (sim channel, fire ticks)
5. If silent visual is on and cmd changed, return `false` from the hook
   (otherwise return the original result)
```

Existing **visual no-recoil** (`FrameStageNotify` at `FRAME_RENDER_START`) is a
**different** cherry: it zeros `m_vecPunchAngle` for the render pass and
restores it afterward so punch does not offset the camera model/view. It does
**not** cancel nospread compensation flicker. Keep both layers:

| Visual problem | Source | Cancellation |
| --- | --- | --- |
| Punch kick on screen | `m_vecPunchAngle` in view setup | Existing FSN zero/restore (`visualNoRecoil`) |
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
| `perfect_nospread` | `perfectNoSpread` | `false` | First-order+iterative cmd compensation |
| `silent_angles` | `silentAngles` | `true` | Queue camera restore for `FRAME_RENDER_START` |
| `visual_norecoil` | `visualNoRecoil` | `true` | FSN punch hide for render |
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

- Prefer `game::SetViewAngles` with executable-slot checks, fail-closed, same
  style as `GetViewAngles`.
- Restore only when a feature actually changed sim angles, and restore from
  `FRAME_RENDER_START` after the original stage handler.
- Perfect nospread only on fire-relevant cmds (`IN_ATTACK` + usable weapon).
- Silent camera suppression does **not** replace `visualNoRecoil`.
- F1 runs **after** norecoil / nospread so the dump shows post-mutation cmd
  angles for that tick.

### 6.7 Silent camera scope and lifecycle (decision)

**Decision: return `false` on every CreateMove tick where this hook changed
`cmd->viewangles` (aimbot, cmd no-recoil, and/or perfect nospread), not only
on fire ticks.**

Rationale:

- Aimbot and no-recoil already write sim angles; without restore they also
  jerk the camera.
- Fire-only restore would still leave aimbot snaps visible.
- The suppression must happen before the engine's post-`CreateMove` copy:

```text
saved = cmd.viewangles at hook entry (post-originalCreateMove)
... features may mutate cmd ...
if silentAngles && cmd.viewangles != saved:
    return false
else:
    return originalCreateMove_result
```

Refinement when aimbot + silent are both on: `saved` should be the **player’s
true look** if aimbot is also silent; if aimbot is *visible*, save post-aimbot
angles as the intended camera. For this tutorial, start with:

```text
intended_camera = angles after originalCreateMove
sim_angles      = intended_camera
sim_angles      = Aimbot(sim) / NoRecoil(sim) / PerfectNoSpread(sim)
cmd.viewangles  = sim_angles
if silentAngles and cmd changed: return false from CreateMove
```

That makes aimbot silent as well when `silentAngles` is on—document it in the
menu as “silent angles (cmd vs camera)”. If a later lesson wants visible
aimbot + silent nospread only, split the saved camera to post-aimbot.

Perfect nospread itself still only **computes** on fire-relevant ticks
(`IN_ATTACK` and weapon can fire); other ticks skip the cone math.

### 6.8 Relation to existing tutorial features

| Feature | Hook stage | Overlap with spread / silent |
| --- | --- | --- |
| Bunny hop / triggerbot | `CreateMove` | buttons only |
| Aimbot | `CreateMove` viewangles | sets **intended** aim before cone compensation |
| No-recoil (cmd) | `CreateMove` | sim punch cancel; stacks before/with nospread |
| Visual no-recoil | `FrameStageNotify` | render punch hide; keep enabled with silent nospread |
| Perfect nospread | `CreateMove` | inverse cone on **cmd** |
| Silent camera | `CreateMove` return value | `false` when cmd angles changed |
| F1 debug | `CreateMove` | dump seed, radii, intended vs cmd vs engine angles |

Recommended order of work:

1. Read-only weapon + seed + radii diagnostic (F1).
2. Prove cone reconstruction offline (log predicted cone vs no mutation).
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
  mode=... clip=... penalty=...   ; each unread if netvar/bounds fail
  GetInaccuracy=... GetSpread=... methodsOk=... usableForCompensation=...
  cmd.number=... stored_seed=... effective_seed=... seed8=... IN_ATTACK=...
  angles cmd=(...) engine=(...) delta=(...)
  predicted pellet0 sx=... sy=... (local RNG)
  compensation residual_deg=... iters=... (first-order+iterative)
  perfect_nospread=on|off silent_angles=on|off pellet_policy=pellet0
```

With `silent_angles=true` and compensation on, a healthy fire tick shows
**cmd** diverging from **engine** while the camera stays on the intended look.

## 8. Signature strategy (if hooks are needed later)

Prefer:

1. Netvars for weapon fields (`m_fAccuracyPenalty`, `m_weaponMode`,
   `m_hActiveWeapon`, `m_iClip1`).
2. Virtual calls on a live weapon object for `GetInaccuracy` / `GetSpread`
   (slot verification via vtable dump of a known weapon).
3. A unique signature for `FX_FireBullets` only if a detour is chosen — anchor
   on the `"FX_FireBullets: weapon alias"` path and the
   `RandomSeed` / `RandomFloat` call cluster; require a single match.

Do **not** hardcode Ghidra absolute addresses into the DLL. Log
module-relative matches the same way as ClientMode / ClientState.

## 9. Runtime validation checklist

- [ ] x64 process + x64 `client.dll`
- [ ] `m_hActiveWeapon` netvar resolves; handle → non-null weapon
- [ ] weapon vtable readable; slots `0xBF0` / `0xBF8` executable
- [ ] `GetInaccuracy` / `GetSpread` return finite floats in a plausible range
      (near 0 when standing still with a rifle after recover; larger when
      moving / mid-spray)
- [ ] `m_fAccuracyPenalty` rises after firing and decays over time
- [ ] Positive `command_number` values change while firing and
      `effective_seed` matches the seed the game publishes after
      `ClientMode::CreateMove` (diagnostic only)
- [ ] Zero-sequence extra samples are reported as unavailable and do not receive
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

x86 weapon fire also `MOVZX` the seed global low byte before
`FX_FireBullets`. Cone math matches (shared inaccuracy sample + per-pellet
spread; `fcos`/`fsin` make right/up assignment explicit).

## 11. Decisions closed in this chapter

| Topic | Decision |
| --- | --- |
| Multi-pellet | Default **`pellet0`**; no “perfect for all pellets” claim |
| Local seed | **`MD5_PseudoRandom(command_number) & 0x7fffffff`, then low byte** |
| TE seed | Full `m_iSeed` for remote FX only; Recv `+0x44` vs object `+0x3C` bases |
| Silent camera | Return `false` for any cmd angle mutation in the hook (§6.7) |
| RNG for predict | **Local** UniformRandomStream; never reseed vstdlib from F1 |
| Compensation | First-order + iterative; log residual degrees; not claimed exact-perfect |
| Right/up | `sx = cosθ·r`, `sy = sinθ·r` |
| Fail-closed | Require methods + clip read success + clip>0 + residual ≤ 1° |

## 12. Implementation status

| Item | Status |
| --- | --- |
| `game::SetViewAngles` (slot 20) | Diagnostic-only; not used for silent camera |
| Weapon resolve + GetInaccuracy/GetSpread | Implemented + bounds; x64 handle ABI corrected |
| mode/clip/penalty read-success flags | Implemented |
| Local RNG cone predict (non-mutating) | Implemented |
| First-order + iterative compensate | Implemented |
| F1 after mutation | Implemented |
| `perfect_nospread` (default **off**) | Implemented |
| `silent_angles` camera suppression | Implemented via the CreateMove return value |
| Offline deterministic math/ABI tests | `tests/weapon_math_tests.cpp`; CTest passes on x86 and x64 |

Still **exploratory** until a local smoke test records residual_deg and
seed8/radii on a live rifle. The offline test target covers command-seed
vectors, zero-sequence handling, `CUserCmd` layout, cone determinism, and
compensation residuals; it does not validate live signatures, vtables, or
object lifetime. Builds and offline tests for x86/x64 pass.

## 13. Reversal record (summary)

```text
Question/behavior: perfect nospread (seed inverse cone on cmd) + silent camera
Binaries: client.dll x64 + x86; engine.dll x64 for view
Ghidra: /source-engine-tutorial/x64/client.dll (0x180000000), /client.dll (0x10000000)
Cone: seed8+1 local stream; sx=cosθ·r, sy=sinθ·r; pellet0
Seed: weapon MOVZX low byte; TE m_iSeed full int (object +0x3C / Recv +0x44)
Compensation: first-order + iterative residual measure
Silent: queue on any cmd angle mutation; restore with SetViewAngles at
FRAME_RENDER_START; nospread math fire-only
Config INI: perfect_nospread=false; silent_angles=true
Static ABI verification: x64 CBaseHandle slot-4 listing and the
`FUN_180152290`/`FUN_1801527B0` input ordering confirmed; runtime smoke test
after the zero-sequence guard is still pending; offline weapon-math tests pass
on both x86 and x64
```

## Definition of done for a *feature* (not yet claimed)

A no-spread tutorial feature is complete only when it also satisfies the
project-wide definition of done: Ghidra evidence, pointer basis, config
provenance, fail-closed reads, F1 diagnostics distinguishing “disabled /
no weapon / unreadable / success”, builds for affected profiles, and a local
smoke test that records residual degrees and what was actually observed.
Label remains **exploratory** until that smoke test is recorded.
