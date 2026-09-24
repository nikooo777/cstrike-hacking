# 006 - Bone ESP and world-to-screen projection

This chapter adds a small, read-only skeleton overlay. It is intentionally
disabled by default. The goal is not to collect a convenient list of bone
offsets; it is to show how to drill from an engine interface to a studio model,
establish the pointer basis for cached transforms, and prove the projection
math before drawing anything.

The implementation targets the x64 Counter-Strike: Source build first. The x86
DLL compiles, but the RenderView and ModelInfo paths still need
their own Ghidra and runtime validation before the feature should be enabled in
the x86 profile.

Use this only with binaries and processes you are permitted to study,
preferably in an offline or local test environment. The overlay is read-only:
it never writes game memory or calls `SetupBones`.

Module-relative addresses describe the August 2026 x64 build. The 2026-09-20
update changed `client.dll`, `engine.dll`, and `vguimatsurface.dll`, but not
the pieces this chapter relies on. RenderView slot 50 is still at
`engine.dll+0x12EBE0` with unchanged code, and its two matrix helpers moved to
`+0xD5600` and `+0xD5990`. ModelInfo slot 28, renderable slot 9, and the
`SetupBones` cache displacements are unchanged. Chapter 004 section 11.1 lists
the new registration, object, and vtable addresses.

## 1. Define the observation

The feature needs four independent results:

1. a final camera view for the current frame;
2. a world-to-projection matrix for that view;
3. a model's bone parent hierarchy;
4. a readable cached matrix for each bone.

The drawing code must be able to distinguish these failures. A valid entity
pointer is not proof that its model, studio header, cached matrix, or camera
matrix is valid. The F1 diagnostic therefore reports view readiness, matrix
readiness, candidate count, resolved hierarchies, and projected line count.

The pipeline is:

```text
captured final CViewSetup
  -> VEngineRenderView014::GetMatricesForView
     -> world-to-projection matrix
        -> entity + 0x8 renderable subobject
           -> GetModel
              -> VModelInfoClient006::GetStudiomodel
                 -> dynamic parent links
                    -> entity-relative cached bone matrix
                       -> world-to-screen
                          -> EndScene draw list
```

The parent links come from the studio header rather than from a hardcoded
player-specific list. That matters because model skeletons can differ and a
bone index is not a stable semantic relationship by itself.

## 2. Select the correct Ghidra program

The relevant programs are:

| Binary | Ghidra program | Language | Image base |
| --- | --- | --- | ---: |
| x64 engine | `/source-engine-tutorial/x64/engine.dll.0` | `x86:LE:64:default` | `0x180000000` |
| x64 client | `/source-engine-tutorial/x64/client.dll` | `x86:LE:64:default` | `0x180000000` |

Record module-relative addresses. Do not copy the `0x180...` Ghidra address
into the DLL, and do not reuse an x86 address because an interface name looks
the same. The runtime address is:

```text
loaded module base + module-relative offset
```

For an interface, prefer the exported registration path over a copied global.
The string, creator, object, vtable, and selected slot should all be visible in
the same binary before the slot is used.

## 3. Drill `VEngineRenderView014`

### 3.1 Start from the interface string

In the x64 engine program, search defined strings for:

```text
VEngineRenderView014
```

The string is at `engine.dll:0x1803934A0`. Follow its references to the
interface-registration thunk at `engine.dll+0x9280`. The listing contains the
RIP-relative setup for the creator, name, and registration list:

```asm
4C 8D 05 19 A2 38 00    ; R8  -> 0x1803934A0, interface name
48 8D 15 82 5F 12 00    ; RDX -> 0x18012F210, creator
48 8D 0D CB F8 6C 00    ; RCX -> interface list
E9 56 71 27 00
```

The creator at `engine.dll+0x12F210` returns the object stored at
`engine.dll+0x4713D8`. Read the first pointer in that object. It points to the
RenderView vtable at `engine.dll+0x3932E0`.

### 3.2 Verify the method slot, not just the object

`GetMatricesForView` is slot 50. Since a pointer is eight bytes in x64, the
entry is at vtable byte offset `50 * 8 = 0x190`, and the entry points to:

```text
engine.dll+0x12EBE0
```

The decompiler names this function `FUN_18012EBE0`. Its two calls are the
important evidence:

```text
FUN_1800D5580(param_3, param_4, param_5, param_2)
FUN_1800D5910(param_6, param_5, param_2)
```

The matching engine SDK declares the call as:

```text
GetMatricesForView(
    view,
    worldToView,
    viewToProjection,
    worldToProjection,
    worldToPixels)
```

The first helper reads the `CViewSetup` values for the camera and constructs
the view/projection matrices. The second constructs the pixel-space matrix.
This confirms both the slot and the x64 member-call ABI. The implementation
uses the world-to-projection output because it makes the overlay independent
of a copied global matrix address.

The configured interface records this provenance in
`config/signatures-x64.ini`:

```ini
[interface.RenderView]
name=VEngineRenderView014
```

The resolver still validates that slot 50 is readable and executable before
calling it. The call is wrapped so a stale interface cannot take down the
process, and every matrix element must be finite before the result is accepted.

### 3.3 The `CViewSetup` overlay must be complete

An overlay that stops at `angles` (`+0x4C`) is enough for the visual no-recoil
hook, but not for `GetMatricesForView`. Ghidra's `FUN_1800D5580` reads the
camera setup at `+0x58`, `+0x5C`, `+0x60`, `+0x64`, `+0x68`, `+0x6C`,
`+0x70`, `+0x74`, `+0x78`, `+0x7C`, `+0x80`, `+0x84`, `+0x85`, `+0x86`, and
the override matrix at `+0x88`.

The matching SDK `CViewSetup` layout confirms these fields:

| Field | Offset |
| --- | ---: |
| `angles` | `0x4C` |
| `zNear` / `zFar` | `0x58` / `0x5C` |
| `zNearViewmodel` / `zFarViewmodel` | `0x60` / `0x64` |
| render-to-subrect flag | `0x68` |
| aspect ratio | `0x6C` |
| off-center flag and bounds | `0x70` / `0x74..0x80` |
| post-processing/cache/override flags | `0x84..0x86` |
| view-to-projection override matrix | `0x88` |

The local overlay includes those fields and asserts a total size of `0xC8`.
This is a general reversal lesson: a structure can appear correct when an
earlier hook reads one field, yet still be invalid for a later callee that
consumes the rest of the object.

## 4. Drill `VModelInfoClient006`

### 4.1 Find the model interface independently

Search the same x64 engine program for:

```text
VModelInfoClient006
```

The string is at `engine.dll:0x1803AE9B0`. Its registration thunk is at
`engine.dll+0x10560` and uses creator `engine.dll+0x1CBD80`. The creator returns
the global object at `engine.dll+0x47AF30`; its first pointer is the vtable at
`engine.dll+0x3AE7C8`.

Do not assume that the RenderView slot number or vtable belongs to ModelInfo.
In the actual vtable, `GetStudiomodel` is slot 28 (`0xE0` bytes) and points to:

```text
engine.dll+0x1CAFC0
```

The decompiler names it `FUN_1801CAFC0` and shows the essential behavior:

```text
if (*(int *)(model + 0x20) == 3)
    return model_cache->lookup(*(uint16_t *)(model + 0x48));
return nullptr;
```

The model type check and the ushort lookup are useful validation clues. A
non-null model pointer alone is not enough; this method must return a readable
studio header for the current model.

The matching `hl2sdk/public/engine/ivmodelinfo.h` header cross-checks the
interface name and method order. The binary vtable and decompiled function are
the proof for this build; the SDK is only a structural cross-check.

## 5. Establish the entity pointer basis

`GetClientEntity` returns the main entity pointer. `IClientRenderable` is an
embedded subobject at `entity + 0x8` in this x64 build. The chain used by the
implementation is therefore:

```text
entity from IClientEntityList
  -> entity + 0x8                (IClientRenderable subobject)
     -> vtable slot 9             (GetModel)
        -> model_t*
           -> ModelInfo slot 28   (GetStudiomodel)
              -> studiohdr_t*
```

The slot-9 `GetModel` method is cross-checked against the matching
`IClientRenderable` declaration. The `entity + 0x8` adjustment is represented
as `sdk::render::kRenderableSubobjectOffset`; it is not scattered through the
feature.

This basis must remain separate from the cached-bone basis. Ghidra's
`C_BaseAnimating::SetupBones` path at `client.dll+0x7B4A0` operates on the
renderable subobject. Its relevant x64 accesses are:

```asm
MOVSXD R8, dword ptr [R14 + 0xB50] ; cached bone count
MOV    RDX, qword ptr [R14 + 0xB40] ; cached matrix pointer
```

Translating from the renderable pointer to the entity pointer gives:

| Value | Renderable-relative | Entity-relative |
| --- | ---: | ---: |
| cached matrix pointer | `+0xB40` | `+0xB48` |
| cached bone count | `+0xB50` | `+0xB58` |

The matrix stride is `0x30` bytes. The position components are at matrix
offsets `+0x0C`, `+0x1C`, and `+0x2C`. The entity-relative values live in
`sdk/client_offsets.h`. `game::GetBonePositions` validates the pointer and the
count (`1..256`), then reads the whole cache with one checked copy of
`count * 0x30` bytes. The overlay checks that each position is finite before
it projects it.

The reusable reversal rule is:

```text
pointer held by the feature
  -> pointer passed to the discovered function
     -> register used by the instruction
        -> displacement shown by Ghidra
```

Never repair a bad read by trying nearby displacements until one produces a
nonzero value.

## 6. Read the hierarchy from `studiohdr_t`

The current matching `hl2sdk/public/studio.h` layout supplies the following
serialized fields:

| Field | Offset | Type / meaning |
| --- | ---: | --- |
| `id` | `0x00` | `0x54534449` (`IDST`) |
| `length` | `0x4C` | serialized header length |
| `numbones` | `0x9C` | number of `mstudiobone_t` records |
| `boneindex` | `0xA0` | relative offset to the bone array |
| `mstudiobone_t::parent` | `0x04` | parent index, or `-1` for a root |
| `mstudiobone_t` stride | `0xD8` | serialized record size |

The code does not treat the SDK struct as a license to dereference arbitrary
memory. It reads the fields individually and rejects:

- a wrong studio ID or non-positive length;
- a count outside `1..256`;
- a non-positive bone index;
- an unreadable complete parent-data range;
- a parent below `-1` or outside the bone count.

The parent array is then used to draw a line only when both the bone and its
parent have valid cached positions. This avoids hardcoding “head is bone 14”
as the skeleton topology. Bone 14 remains useful to the aimbot because that is
its separately documented target choice, not because it defines every model's
hierarchy.

## 7. Prove world-to-screen before drawing

`GetMatricesForView` fills Source `VMatrix` values, declared locally as
`sdk::render::Matrix4x4 { float m[4][4]; }` and asserted to be `0x40` bytes.
The implementation follows the SDK `VMatrix` convention: `m[row][col]`, with
the point treated as a column vector (`clip = M * (x, y, z, 1)`). Direct3D's
own convention multiplies row vectors, which would transpose every index below.
That is the first thing to suspect if projected points appear mirrored or
rotated.

For a world point `(x, y, z, 1)`, calculate:

```text
clipX = m[0][0]x + m[0][1]y + m[0][2]z + m[0][3]
clipY = m[1][0]x + m[1][1]y + m[1][2]z + m[1][3]
clipW = m[3][0]x + m[3][1]y + m[3][2]z + m[3][3]

ndcX = clipX / clipW
ndcY = clipY / clipW
screenX = viewportX + (ndcX + 1) * viewportWidth  / 2
screenY = viewportY + (1 - ndcY) * viewportHeight / 2
```

Row 2 produces depth, which a 2D overlay does not need. The `1 - ndcY` term
flips the axis: normalized device coordinates grow upward, while screen pixels
grow downward.

The viewport comes from `IDirect3DDevice9::GetViewport` at `EndScene` time, not
from the `x`/`y`/`width`/`height` fields in `CViewSetup`. Those describe the
view the engine rendered, which can be scaled or a sub-rectangle. The D3D
viewport describes the surface the overlay actually draws on.

The helper rejects non-finite matrix or point values, invalid viewports, and
`clipW <= 0.001`. The last check prevents behind-camera points and near-plane
division from producing huge lines. Screen bounds are not required for the
calculation itself: ImGui clips lines that are partly off screen, and the
diagnostic counts every line whose two endpoints projected successfully.

The offline `projection_tests` target first uses an identity matrix to verify
the center and viewport corners, then checks invalid viewports and a point with
non-positive `w`. `ctest` runs it together with `weapon_math_tests`. These
tests prove the pure convention used by the feature; they do not prove that a
future engine returns the same matrix orientation.

## 8. Choose a coherent render lifetime

The final camera can differ from the input camera because the visual
no-recoil hook adjusts `CViewSetup::angles` (chapter 005, section 9.4). The
`OverrideView` hook therefore calls the original function, applies the visual
punch correction when it is enabled, and only then captures the `CViewSetup`.
`EndScene` consumes that snapshot when it requests the world-to-projection
matrix, once per frame. This keeps the overlay aligned with the camera that is
actually rendered, including the recoil-free view when visual no-recoil is on.

Bone caches are timing-sensitive. `CreateMove` and `EndScene` do not necessarily
observe the same entity state, so the overlay reads the cache at draw time and
fails closed when it is not ready. It does not call `SetupBones` from the draw
hook, scan signatures in the render loop, or assume that a cached matrix is
fresh merely because its address is readable.

The hooks run in this order:

```text
OverrideView
  -> original camera setup
  -> optional visual no-recoil on CViewSetup::angles
  -> capture the final CViewSetup (mutex-protected copy)

EndScene
  -> poll Insert, initialize ImGui on the first call
  -> start the ImGui frame
  -> if the menu is open, submit the menu wheel
  -> read the D3D viewport and call the ESP, which returns early when disabled
  -> render ImGui
  -> call the original EndScene
```

The menu is submitted first, but the skeleton goes to ImGui's background draw
list, which renders beneath every window, so the menu wheel (chapter 007) stays
readable over it. The ESP runs whether or not the menu is open.

The code does not assume that `OverrideView`, `EndScene`, and `CreateMove` run
on the same thread. The view snapshot and the ESP diagnostics are copied under
their own mutexes, which has two consequences worth knowing:

- F1 prints from `CreateMove`, so its `bone esp:` line reports the most recent
  `EndScene`, not the current tick;
- the bone cache itself is read without any lock. If the game updates a cache
  on another thread during a read, a line can be distorted for that frame. The
  reads are still checked for readability and finite values before anything is
  drawn.

The view snapshot expires. `game::GetCapturedViewSetup` rejects a snapshot that
`OverrideView` has not refreshed within 500 ms, so a loading screen, a
disconnect, or any other stretch without world rendering reports
`view=unavailable` instead of projecting through the last camera of the old
session.

The original D3D9 call remains part of the hook. Chapter 007 section 7.1 adds
the device reset handling. Window-procedure restoration remains a general
overlay concern and must be rechecked if the renderer changes.

### 8.1 Select and draw candidates

`features/bone_esp.cpp` iterates player indices `1` through `MAXPLAYERS - 1`
and reuses the aimbot's `game::IsValidTarget`. A candidate must be:

- a non-null entity in a player slot;
- alive;
- not dormant, according to `IClientNetworkable::IsDormant` (chapter 004,
  section 9.2);
- an enemy, meaning on a playing team other than the local player's. This also
  excludes the local player, spectators, and unassigned players.

There is no visibility trace, so skeletons are drawn through walls. Adding one
would reuse the chapter 004 `TraceRay` path.

For each candidate, the parent array from section 6 defines the lines. A line
is drawn from a bone to its parent only when both indices are inside the cached
bone count, both positions are finite, and both endpoints pass the projection
checks. A root bone (`parent == -1`) has no line. The F1 counters follow the
same filter: `candidates` counts valid targets, `hierarchies` counts those
whose studio parent array was read, and `projectedLines` counts lines that
reached the draw list.

## 9. Implementation and configuration

Resolution stays in `game/` and math stays in a pure helper:

| File | Responsibility |
| --- | --- |
| `sdk/render_view.h` | interface slot constants, matrix type, call typedefs, renderable basis |
| `sdk/studio_model.h` | checked serialized studio offsets and limits |
| `game/interfaces.cpp` | named interface lookup and `GetMatricesForView` validation |
| `game/model.cpp` | renderable/model/studio chain and one-copy parent-table read |
| `game/render_state.cpp` | mutex-protected final view snapshot |
| `game/player.cpp` | shared target filter and whole-cache bone position reads |
| `math/projection.cpp` | finite world-to-screen calculation in the `VMatrix` convention |
| `hooks/override_view.cpp` | captures the final view after the visual no-recoil correction |
| `hooks/end_scene.cpp` | D3D viewport, ImGui frame, and the ESP call |
| `tests/projection_tests.cpp` | offline projection convention and rejection tests |
| `features/bone_esp.cpp` | candidate filtering, cached positions, and draw lines |

The architecture-specific profiles contain optional interface provenance:

```ini
[features]
bone_esp=false

[interface.RenderView]
name=VEngineRenderView014

[interface.ModelInfo]
name=VModelInfoClient006
```

The defaults stay false until runtime validation is complete. The menu toggle
can enable the feature for a local test session without making injection depend
on it. If either interface or any pointer chain fails, the diagnostic reports
the failure and draws nothing.

## 10. Runtime validation

Build and load the architecture-matched DLL, then use the following order:

1. Confirm the normal interface, netvar, ClientState, and bone-cache lines are
   healthy.
2. Press **Insert**, click **Visuals**, then **Bone ESP**, and close the menu.
   With the menu open, **Status**, then **ESP** shows the same stages live.
3. Press **F1** while an enemy player is present.
4. Check the Bone ESP line:

   ```text
   bone esp: enabled=yes viewport=ready view=ready matrix=ready candidates=...
     hierarchies=... projectedLines=... model=0x... studio=0x... bones=...
   ```

   `model`, `studio`, and `bones` describe the last candidate whose hierarchy
   resolved in that frame, not every candidate.

5. Interpret failures in order:

   | F1 value | Meaning | Next check |
   | --- | --- | --- |
   | `enabled=no` | The toggle was off during the last `EndScene` | Enable **Visuals**, then **Bone ESP** in the menu |
   | `viewport=unavailable` | `IDirect3DDevice9::GetViewport` failed or returned an empty viewport | Check the D3D9 device and window state |
   | `view=unavailable` | No `OverrideView` snapshot in the last 500 ms | Confirm the `OverrideView` hook is installed and logged, and that a map is loaded |
   | `matrix=unavailable` | RenderView resolution, slot 50, the call ABI, or matrix validation failed | Section 3 |
   | `candidates=0` | No enemy passed the alive/dormant/team filter | Section 8.1 and the aimbot target counts in the same dump |
   | candidates but `hierarchies=0` | Renderable basis, `GetModel`, ModelInfo, `GetStudiomodel`, or studio-header validation failed | Sections 4 to 6 |
   | hierarchies but `projectedLines=0` | Cached matrix timing or contents, parent positions, or the projection convention | Sections 5 and 7 |

   The line reports the last `EndScene`, so it can lag the dump by a frame.

The first acceptance target is not "the skeleton looks plausible". It is a
diagnostic where every stage is readable, the line count is stable while the
target is visible, and the lines move with the target and the camera. Compare a
known local origin and a point in front of the camera before changing matrix
row/column order.

### 10.1 Recorded result

The x64 local smoke test drew valid enemy skeletons from the cached matrices.
The bone-cache path it depends on was validated separately (chapter 004,
section 9.1: readable, usable, `count=50`). No F1 `bone esp:` line was recorded
for the drawing run, so there is no reference line yet (section 11).

## 11. What is and is not proven

Proven for the August x64 binaries:

- RenderView registration, object, vtable, slot 50, and callee;
- ModelInfo registration, object, vtable, slot 28, and `GetStudiomodel` logic;
- Renderable `GetModel` slot 9 and the `entity + 0x8` pointer basis;
- SetupBones cache fields after translating from renderable to entity basis;
- studio parent-field offsets against the matching SDK;
- finite, checked projection math through offline tests;
- x86 and x64 MSVC builds of the implementation and projection tests;
- a local x64 smoke test that drew enemy skeletons (section 10.1).

Also verified statically for the 2026-09-20 binaries (chapter 004, section
11.1): the same slots, callees, pointer basis, and cache displacements.

Still requiring a new reversal or runtime test:

- a runtime smoke test on the 2026-09-20 binaries;
- a recorded F1 `bone esp:` reference line, with the candidate, hierarchy, and
  line counts for a known number of enemies, and lines that track the targets
  and the camera;
- x86 RenderView/ModelInfo slots and their calling conventions;
- cache freshness at every renderer state, and device-reset behavior;
- visibility, interpolation, and server-authoritative hitbox semantics;
- any future engine/client build with different interface versions or layouts.

The correct response to a failed overlay is to collect the missing static or
runtime evidence and update this chapter. Do not select a nearby slot, offset,
matrix convention, or bone list because it happens to produce lines.
