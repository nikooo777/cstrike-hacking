# Reversing handoff for future agents

This file is the working handoff for agents continuing the tutorial. Read it
before changing a signature, private field, hook, or feature. It records the
parts of the x64 port that are easy to get subtly wrong and the evidence that
must be collected before an offset is treated as real.

The project is an educational, local Source-engine experiment. Its offsets and
patterns describe particular binaries on disk; they are not a promise that a
future game update will have the same layout. When a binary changes, repeat the
Ghidra and runtime validation instead of trying nearby values until something
appears to work.

## Where the truth lives

Keep these three layers in agreement:

| Layer | Purpose |
| --- | --- |
| `config/signatures.ini` and `config/signatures-x64.ini` | Architecture-specific patterns, operand decoding, feature defaults, and provenance. |
| `Nikooo777/` | Resolvers, ABI declarations, checked memory access, hooks, and features. |
| `aidocs/001` through `aidocs/005` | The evidence-based tutorial narrative: how a value was found and why the implementation uses it. |

The x64 profile is selected by `CMAKE_SIZEOF_VOID_P` in `CMakeLists.txt` and
copied beside the built DLL as `signatures.ini`. Do not edit only the copied
build-directory file and expect the change to survive a reconfigure. Edit the
source profile under `config/`.

The current architecture split is intentional:

- x86 uses the legacy profile and x86 declarations/offsets.
- x64 uses `config/signatures-x64.ini`, source-built 64-bit MinHook, and the
  x64 module set under the game's `bin/x64` directories.
- A module is still named `client.dll` or `engine.dll` at runtime even when its
  on-disk path is `cstrike/bin/x64/client.dll` or `bin/x64/engine.dll`.

The relevant chapters are [001: signatures](aidocs/001_signature-scanning-and-offsets.md),
[002: netvars](aidocs/002_netvars-and-entity-offsets.md),
[003: globals and input](aidocs/003_global-addresses-and-inputs.md),
[004: x64 migration](aidocs/004_x64-migration-and-abi.md), and
[005: no-spread / weapon accuracy](aidocs/005_no-spread-and-weapon-accuracy.md).
Add a new numbered chapter when a reversal becomes a tutorial step; update this
handoff when the result is a reusable rule for later work.

## Before opening Ghidra

1. Confirm the process and every module belong to the same architecture. The
   old binaries use PE Machine `0x014C` / optional header `0x010B`; the current
   x64 binaries use `0x8664` / `0x020B`.
2. Use a separate Ghidra program for x64. The useful x64 language is
   `x86:LE:64:default` and the sample image base is `0x180000000`.
3. Record the module name, binary version/hash if available, Ghidra image base,
   and module-relative address. Runtime ASLR addresses are diagnostic only:

   ```text
   runtime address = loaded module base + module-relative offset
   ```

4. Never reuse an x86 Ghidra address, data type, structure overlay, or pattern
   merely because the symbol or interface has the same name.
5. If Ghidra MCP is available, record the program, address, instruction bytes,
   references, and the relevant register/data-flow observation. A decompiler
   guess without the listing/assembly evidence is not a confirmed reversal.

## x64 ABI gotchas

### Pointer width is only one part of the change

On MSVC x64, pointers, `std::uintptr_t`, `size_t`, and vtable entries are eight
bytes. `int`, `DWORD`, `float`, and `long` are still four bytes. Do not turn
every field into a 64-bit value. Do turn every pointer field and pointer-sized
intermediate into `std::uintptr_t` or an actual pointer type.

Pointer fields introduce padding and move later fields. This is why the x64
`ClientClass`, `RecvTable`, and `RecvProp` metadata layouts have different
internal offsets even when the logical netvar name is unchanged. Keep the
compile-time layout assertions in the SDK headers; if one fails, stop and fix
the ABI declaration rather than compensating with a new field offset.

### Calling conventions and hook prototypes

The Microsoft x64 ABI uses one normal calling convention for this code. The
first integer/pointer arguments are passed in `RCX`, `RDX`, `R8`, and `R9`, and
floating-point arguments use the corresponding XMM registers. The caller also
provides the required home/shadow space. x86 `__thiscall`/`__fastcall`
distinctions still matter, but x64 annotations do not recreate the x86 ABI.

Keep architecture-specific function typedefs explicit in the hook headers.
Match all of the following before installing a hook:

- return type and width;
- parameter order and whether a parameter is a pointer, integer, float, or
  reference;
- whether the target is a member call and where `this` arrives;
- the vtable slot and the actual function's prologue/call sites.

An x86 MSVC error such as “the value of ESP was not properly saved” usually
means a wrong calling convention or prototype. The source line named by the
runtime check may only be where the corrupted stack is noticed. On x64, a
prototype mismatch often presents instead as bad register values, corrupted
stack/home-space use, or a crash after returning from the hook.

`CreateMove` is a good example. The x86 hook has the `this`/`edx` shape needed
by its thunk; the x64 hook receives the normal x64 arguments. Do not copy the
x86 hook signature and only change the pointer types.

#### Small non-trivial value parameters can still be ABI-sensitive

`CBaseHandle` is a useful trap. In the matching Source SDK it is a four-byte
class with user-defined constructors/copy operations, and the entity-list
methods take it **by value**. On MSVC x64 that is not equivalent to declaring
the parameter as `std::uint32_t`: the caller passes a temporary object's
address in `RDX` for the class form. The current x64 `client.dll` confirms
this at `VClientEntityList003` slot 4 (`client.dll+0xDF700`):

```asm
MOV EAX, dword ptr [RDX]       ; read CBaseHandle::m_Index
LEA RDX, [RSP + 0x30]          ; make the next by-value temporary
MOV dword ptr [RSP + 0x30], EAX
CALL qword ptr [RAX + 0x10]    ; GetClientUnknownFromHandle
```

The static interface evidence is `VClientEntityList003` at
`client.dll+0x42D770`, its object/vtable path at `+0x6498C8` / `+0x42D7B0`,
and slot 4 at `+0xDF700`. Mirror the real non-trivial class, keep its size
assertion at four bytes, and do not replace it with a raw integer merely
because the stored handle has 32 bits. A valid-looking handle can otherwise
be read successfully while every lookup returns null.

### Vtables and subobjects

Virtual slots are indexed by pointer-sized entries. A slot number can remain
stable while the vtable address and every vtable entry change. Verify the
loaded vtable entry is readable and executable before calling it.

Multiple inheritance and embedded interface subobjects are especially
important. A pointer returned by `GetClientEntity` is not necessarily the same
pointer passed to every virtual method. The entity's `IClientRenderable`
subobject in this build is at `entity + 0x8`; methods operating on that
subobject see an adjusted `this` pointer.

Do not apply a displacement from a disassembly directly to the pointer held by
the feature until the pointer basis is identified.

## RIP-relative operands and signature configuration

x86 commonly embeds an absolute `imm32` address. x64 code commonly uses a
signed 32-bit displacement relative to the next instruction:

```text
target = instruction_address + instruction_length + sign_extend(rel32)
```

The four displacement bytes are not a complete x64 pointer. In the config:

- `operand=rip_rel32` selects this decoding;
- `operand_offset` points to the four displacement bytes in the match;
- `instruction_offset` points to the start of that RIP-relative instruction;
- `instruction_length` is the length used in the formula;
- `indirections` describes additional pointer reads after decoding.

Interpret `indirections` from the instruction semantics, not from intuition:

```text
LEA RBX, [RIP + rel32]       -> decoded object address, often indirections=0
MOV RCX, [RIP + rel32]       -> decoded global-slot address; often read once
                               to obtain the object pointer
```

The current x64 ClientMode pattern is a direct object address and uses
`indirections=0`. The x64 ClientState initializer likewise resolves the live
object directly. Other builds may use a slot, so always inspect the bytes.

Signature rules:

1. Anchor the pattern on semantic code: an interface access, constructor
   sequence, vtable dispatch, or distinctive call path.
2. Wildcard relocatable addresses and displacements, but keep opcodes, stable
   register/data-flow bytes, and enough surrounding control flow to make the
   match unique.
3. Require a unique match. If wildcarding creates multiple matches, tighten
   the pattern or find a better anchor; do not select the first result.
4. Decode the operand and validate the result as the expected kind of object:
   readable object, plausible vtable, executable method, or valid data region.
5. Keep `source`, `source_readme`, `source_video` where applicable, and a
   plain-language `discovery`/`notes` entry in the config.
6. Log the module-relative match offset. It lets the next reversal compare a
   runtime report with Ghidra without confusing ASLR addresses.

The x64 ClientState pattern is deliberately more exact than a generic C++
initializer because the short wildcarded form matched many functions. That is
an acceptable build-specific trade-off when the uniqueness check and the
re-dump procedure are documented.

## Finding interfaces and methods

Prefer a named interface or a netvar over a copied global address when one
exists. A reliable interface workflow is:

1. Search the correct module for the interface string, such as
   `VClientEntityList003` or `VEngineClient014`.
2. Follow its references to `CreateInterface`/`InterfaceReg` and identify the
   returned object.
3. In x64, model the registry record as three eight-byte fields: create
   function at `+0x00`, name pointer at `+0x08`, and next pointer at `+0x10`.
4. Check the vtable in the actual binary. Cross-check method order against the
   matching Source SDK header, but do not treat the header as proof for a
   different branch or build.
5. Validate the selected slot's function address before invoking it.

Slots currently verified for the sample x64 build, counting from zero:

| Object/interface | Method | Slot |
| --- | --- | ---: |
| `VClient017` | `GetAllClasses` | 8 |
| `VClient017` | `IN_ActivateMouse` / `IN_DeactivateMouse` | 14 / 15 |
| `VClient017` | `FrameStageNotify` | 35 |
| `VClientEntityList003` | `GetClientEntity` | 3 |
| `ClientMode` | `OverrideView` | 16 |
| `ClientMode` | `CreateMove` | 21 |
| `VEngineClient014` | `GetViewAngles` | 19 |
| `EngineTraceClient003` | `TraceRay` | 4 |
| `IClientUnknown` | `GetClientNetworkable` | 4 |
| `IClientNetworkable` | `IsDormant` | 8 |
| `VGUI_Surface030` | `SetCursor` | 51 |
| `VGUI_Surface030` | `UnlockCursor` / `LockCursor` | 61 / 62 |
| `VGUI_Surface030` | `CalculateMouseVisible` / `IsCursorLocked` | 93 / 104 |
| `IDirect3DDevice9` | `EndScene` | 42 |

These are evidence for the current sample, not universal constants. The x64
engine's view-angle path is obtained through `VEngineClient014`; do not reuse
the old x86 `ClientState + 0x4B84` overlay on x64.

Source cursor ownership is decided before rendering. In the matching x64
engine, `CEngineVGui::Simulate` at `engine.dll+0x227620` calls
`VGUI_Surface030::CalculateMouseVisible`, checks `IsCursorLocked`, then calls
`VClient017::IN_ActivateMouse` or `IN_DeactivateMouse`. An ImGui window is not
a VGUI popup, so changing visibility or unlocking once in `EndScene` loses to
the next engine simulation. The current menu hook intercepts verified surface
slot 62: it calls the original while closed and substitutes slot 61 plus the
arrow cursor while open. Re-verify the surface object, slots, and engine caller
together after a VGUI or engine update.

The current visibility path uses `EngineTraceClient003::TraceRay` slot 4 with
Source's `MASK_VISIBLE` (`0x6081`). The local ABI declaration preserves the
`Ray_t` layout and `CGameTrace::fraction` at `+0x2C`, and its filter skips the
local/candidate entities. Re-check the interface, slot, filter entity basis,
and trace layouts together after an engine update; a readable interface alone
does not prove that a visibility result is meaningful.

## Netvars, private fields, and pointer basis

Netvars are a good source for replicated entity fields. They are not a magic
replacement for every private/client-only value:

- use the RecvTable walker for networked properties;
- locate client-only fields, caches, and helper state through code/data-flow;
- name the declaring table or function in logs and documentation;
- record whether an offset is relative to the main entity, a subobject, a
  member inside a nested object, or a pointer target.

The x64 crosshair target (`m_iIDEntIndex`) is currently a client-only field at
`player + 0x1B20` for this build. Dormancy is not treated as a guessed field in
the x64 path: it is queried through the verified
`IClientNetworkable::IsDormant` interface method. This distinction matters
when an old hardcoded stub silently marks every entity dormant.

### Bone-cache case study: the offset is not the whole answer

The most important x64 reversal lesson in this repository came from
`C_BaseAnimating::SetupBones`:

```asm
MOVSXD R8, dword ptr [R14 + 0xB50] ; cached bone count
MOV    RDX, qword ptr [R14 + 0xB40] ; m_CachedBoneData.Base()
```

Ghidra is describing `R14`, the renderable subobject passed to `SetupBones`.
The feature starts with the main entity pointer from `GetClientEntity`, and
the renderable subobject is `entity + 0x8`. Therefore the currently validated
entity-relative fields are:

```text
entity + 0xB48 -> cached bone matrix pointer
entity + 0xB58 -> cached bone count
```

The matrix stride is `0x30`; bone 14's position is at matrix offsets
`+0x0C`, `+0x1C`, and `+0x2C`. The important rule is not these particular
numbers—it is to establish the `this` pointer basis before translating a
displacement.

For any private field, write down this small chain before coding:

```text
pointer held by feature
  -> pointer passed to the discovered function
     -> register used by the disassembly
        -> field displacement shown by Ghidra
```

Then validate the resulting pointer, count, stride, and sample values with
`mem::IsReadable`/`mem::ReadValue`. A readable address can still be the wrong
object, so add plausibility checks and fail closed. Do not “fix” a bad read by
trying `+0xB40`, `+0xB48`, and `+0xB50` until one looks nonzero without recording
the pointer-basis evidence.

Bone data is also timing-sensitive. `CreateMove` can run before the desired
entity has refreshed its cache. A read-only diagnostic should report pointer
readability, count, matrix readability, and whether the values are usable
before a feature depends on them.

## Runtime validation checklist

Every new reversal should have both static evidence and a small runtime test.
The minimum useful checks are:

- the expected module is loaded and has the expected architecture;
- the signature match count is exactly one;
- the decoded address is in the expected module or data region;
- object pointers are readable and their vtable is readable;
- selected vtable entries are executable;
- entity pointers are non-null and fields have plausible ranges;
- pointer chains and counts are checked before dereferencing;
- diagnostics distinguish “feature disabled”, “no valid target”, “target data
  unreadable”, and “data read successfully but application failed”.

Use the existing F1 debug dump as the model: it prints module bases,
module-relative matches/offsets, resolved interfaces, netvars, ClientState,
view angles, dormancy, bone-cache state, and local position. Keep expensive
scans and verbose logging out of the per-tick path; a one-shot diagnostic is
usually enough.

When a resolver fails, fail closed. A null result or a disabled feature is much
easier to investigate than a guessed pointer that corrupts the process.

## Ghidra-to-code workflow for the next reversal

Use this sequence for no-spread, ESP, or any new private field:

1. Define the observation first: what value or behavior is needed, at what
   lifecycle stage, and whether the feature only reads it or mutates it.
2. Find a semantic anchor in the correct x64 module: interface string, RTTI,
   SDK method, diagnostic string, constructor, or an already verified caller.
3. Follow cross-references and inspect the listing around the access. Identify
   registers, pointer adjustments, operand kinds, call targets, and the real
   function prototype.
4. Cross-check the method order or structure declaration against the matching
   Source SDK, then verify the actual binary vtable/data flow.
5. Build the narrowest unique signature and add the correct operand decoder and
   indirection count to the architecture-specific config.
6. Put resolution in `game/`/`memory/`, not in a feature. Expose a small
   checked API to the feature and keep x86/x64 differences local.
7. Add a read-only F1 diagnostic and runtime plausibility checks before adding
   mutation or drawing.
8. Update the next numbered `aidocs/` chapter with the Ghidra address,
   instruction evidence, pointer basis, config entry, and runtime result.
9. Run `git diff --check`, build the affected profile, and build both profiles
   when common headers or architecture conditionals changed. Run the offline
   CTest targets when deterministic math or ABI declarations are involved;
   they exercise the same pure implementation used by the DLL.
10. Perform a permitted local smoke test and record the expected log. Only then
    enable a new feature by default.

## Preparing for a no-spread tutorial

Read [aidocs/005](aidocs/005_no-spread-and-weapon-accuracy.md) before writing
feature code. The tutorial target is **perfect nospread** (seed-based inverse
cone on `CUserCmd`) plus **silent visual suppression** (return `false` so the
CreateMove caller does not copy compensated angles into the camera), composed
with the **visual no-recoil** camera correction in ClientMode slot 16.

```text
command_number -> MD5_PseudoRandom -> CUserCmd.random_seed -> prediction seed global
  -> movement applies one CSS punch-decay tick
  -> weapon slot 384 applies one pre-fire m_fAccuracyPenalty decay tick
  -> weapon fire uses seed8 = random_seed & 0xFF
  -> GetInaccuracy / GetSpread at fire-time state
  -> CS fire path: RandomSeed(seed8+1); polar inaccuracy + spread samples
  -> keep GetInaccuracy and GetSpread as separate radii
  -> helper pair is (low=sin, high=cos); fire consumes (right,up)=(high,low)
  -> fire angles = cmd angles + 2*punch
  -> dir = forward + right*(x*spreadX) + up*(y*spreadY)

CreateMove: input -> desired aim -> config-aware fire-space recoil/spread
  -> one final cmd angle -> return false whenever command no-recoil mutates
     the angle, or when silent_angles hides another mutation
OverrideView: call original -> subtract one punch from CViewSetup::angles
```

The x64 input caller has two relevant paths. `FUN_180152290` builds the real
ring-buffer command, writes `command_number` at `cmd + 0x08`, calls
`ClientMode::CreateMove`, and writes `random_seed` at `cmd + 0x38` afterward.
`FUN_1801527B0` calls the same hook with a temporary extra-mouse-sample command
whose sequence remains zero. Treat `command_number == 0` as an unavailable
seed, not as permission to use `MD5_PseudoRandom(0)`; the hook skips all
feature mutation on that temporary command and defers an F1 dump to the next
real command. Otherwise every extra sample can hide a seed/order bug. The
runtime diagnostic should show positive, changing command numbers during a
magazine and identify zero-sequence samples explicitly.

Do not start by searching for an old “no spread offset”. Verified anchors
(see aidocs/005 for x86/x64 tables):

- x64: penalty `+0xCB0`, mode `+0xCAC`, active weapon `+0x11A0`, vtable
  `GetInaccuracy`/`GetSpread` **slots 382/383** (bytes `+0xBF0`/`+0xBF8`),
  `UpdateAccuracyPenalty` slot 384 (`+0xC00`, `client.dll+0x2367D0`),
  weapon id slot 371 (`+0xB98`), and next-penalty weapon-info fields
  `+0x8CC/+0x8D0/+0x8D4/+0x8D8`;
  pre-fire decay uses weapon-info baselines `+0x8E4/+0x8EC/+0x904`,
  recovery times `+0x91C/+0x920`, and `CGlobalVarsBase+0x1C`;
  the accuracy-info lookup helper at `client.dll+0x4D700` reads the ushort at
  `weapon+0xC62`; do not substitute virtual weapon-id slot 371, which is the
  separate id passed to the fire call;
  one validated capture reported fire id 27, info index 46, predicted/actual
  inaccuracy `0.0282658`, matching fire angles, and matching cone offsets;
  branch-2 `GetInaccuracy` also adds a post-movement speed term from
  absolute velocity `player+0x1A8/+0x1AC`, remapped across 34%-95% of weapon
  speed and scaled by weapon-info `+0x914+mode*4`;
  paired stationary/moving captures showed the movement term matching at
  fire time while the same penalty-only error remained in both cases;
  the optional `UpdateAccuracyPenalty` entry detour records exact pre/post
  `+0xCB0` and `CGlobalVarsBase+0x1C` values before changing the formula;
  the live CSS punch-decay override is movement vtable slot 15 at
  `client.dll+0x1F5AE0`, consuming `player+0x127C` and the same tick interval;
  current CS fire uses `seed8+1` and separate polar radii;
  client `+0x1FFA30` and server `+0x3252E0` are the current CS fire
  references; generic `+0x4FF30` / `+0x137890` are a different path;
  multi-pellet policy **pellet0**
- x86: same slots 382/383 (bytes `+0x5F8`/`+0x5FC`); field offsets differ
- engine: `SetViewAngles` slot 20 for diagnostics; silent mode returns `false`
  on any cmd angle mutation so the CreateMove caller leaves the camera alone

Start with F1 after mutation: raw and predicted fire-time radii, pre-fire
penalty baseline/recovery/tick interval, current/predicted/actual punch,
weapon id, shots fired, next penalty, seed8, local-stream `(sx,sy)`, residual
degrees, cmd-vs-engine angle delta, and `cmd_fire` basis. The one-shot
`FX_FireBullets` capture must show near-zero predicted radius delta and must
reconstruct the actual fire angle from fire source plus `2*actual punch`.
Do not use a fixed wall cluster to judge perfect nospread while command
no-recoil is disabled: natural punch moves the fire angle. Enable both stages
for that observation, or compare every impact to its recoil-adjusted angle.
Keep `perfect_nospread=false` until
residuals look plausible. Compensation is first-order+iterative, not claimed
exact-perfect. A local change that looks correct can still disagree with the
server; document observations only.

The command writers are intentionally composed, not called as independent
angle mutations. `game::ComposeShotAngles` changes its result with the
configuration: no-recoil off preserves the natural `+2*punch` fire basis,
no-spread compensates around that basis, no-recoil on removes punch through
`command=desired-current_2punch`, and enabling both does both in fire space.
Ghidra confirms that `FUN_180054450` adds only `1*punch` to the render view,
while the fire path uses `2*punch`; no non-silent command recurrence can match
both. Whenever command no-recoil changes the cmd, return `false` so the caller
does not copy the fire-compensated angle into the camera. `silent_angles`
continues to control camera suppression for aim/no-spread mutations when
command no-recoil is not applied. If live punch or weapon/seed data is
unreadable, that stage reports unavailable and the pipeline keeps the last
valid stage. The F1 dump prints `aim`, `recoil`, and `spread` stage status plus
`desired`, `fire_base`, and final `cmd` angles.

Do not hide punch by writing zero around `FRAME_RENDER_START`. Ghidra shows
x64 `FrameStageNotify` at `client.dll+0xD59B0` dispatching stage 5 to
`client.dll+0xD6D70`, which runs client entity, temporary entity, and particle
simulation. The verified render-only path is ClientMode slot 16: x64
`client.dll+0xE67C0`, x86 `client.dll+0xF6220`, with
`CViewSetup::angles` at `+0x4C`. The hook calls the original first, subtracts
one current punch from the view angles, and leaves player punch state
untouched.

## Preparing for a visual ESP tutorial

Treat ESP as a pipeline, not as a collection of offsets:

```text
checked game data -> world-to-screen projection -> render-stage drawing
```

Before drawing entities, reverse and validate:

- a reliable entity/origin/eye/bone data source;
- dormancy, team, life-state, and visibility semantics;
- a world-to-screen matrix or named engine camera/view interface;
- matrix row/column order, coordinate convention, clip-space `w`, and viewport
  dimensions/scaling;
- a render lifecycle where bone data and camera data are coherent.

Prefer a discovered engine/view interface or matrix reference over a guessed
global. Test projection with the local player's known origin and a point in
front of the camera. Check the behind-camera/near-zero-`w` case and screen
bounds before attempting boxes or skeletons.

Keep entity collection in `game/`, projection in a small math/helper layer,
and drawing in `features/`/the render hook. Do not scan signatures or walk all
entities inside `EndScene`. Begin with a single read-only local point or box,
then add enemy/team filtering, bones, visibility, and labels one at a time.

The current D3D9 path uses `EndScene` slot 42 and initializes ImGui lazily.
Future drawing work must preserve the original device call, account for device
reset/lost-device behavior, and restore the window procedure during unload.
If a render-stage hook is used to refresh bones, confirm its stage timing in
Ghidra/runtime rather than assuming `CreateMove` and `EndScene` observe the
same entity state.

### CreateMove is not the final camera write

The matching Source client path invokes `ClientMode::CreateMove` and then the
caller copies `cmd->viewangles` into the engine view only when the hook returns
true. A `SetViewAngles` call made inside a `CreateMove` detour is consequently
too early to be a reliable silent-camera restore; the caller overwrites it
before rendering, and a later render-stage restore can race extra input
samples. When command no-recoil changed command angles, return `false`
regardless of the silent-angle toggle. For other command mutations, do so when
silent mode is enabled. The command still continues through seed assignment,
verification, and networking while the caller leaves the render camera alone.
This lifecycle detail explains the characteristic steadily spinning view while
a per-tick inverse-cone feature is active. Also remember that the hook runs
before the engine writes `random_seed`, so derive it from a positive
`command_number` with the same `MD5_PseudoRandom` rule. A zero-sequence extra
sample is not a real firing command and must fail closed.

## Common symptoms and likely causes

| Symptom | First suspicion |
| --- | --- |
| Signature not found | Wrong architecture/module, stale binary, or a pattern that is too specific. |
| Several matches | Too many wildcards or a generic compiler-generated initializer. Tighten it using stable control flow. |
| Decoded address is zero/garbage | Wrong RIP instruction length, signedness, operand offset, or indirection count. |
| Pointer is readable but fields are nonsense | Wrong pointer basis, adjusted `this`, stale object, or structure padding. |
| Immediate crash after a hook | Wrong vtable slot, function prototype, return type, or x86 calling convention. |
| All x64 entities are dormant | A placeholder field/accessor is being used instead of `IClientNetworkable::IsDormant`. |
| Bone cache is unavailable | Wrong entity-versus-renderable basis, invalid pointer/count, or the cache is not ready at that stage. |
| All candidate targets are invisible | EngineTrace interface/slot, Ray_t/CGameTrace ABI, filter entity basis, or trace timing is wrong. |
| View angles are zero | Wrong interface/slot or an obsolete ClientState overlay. |
| Menu opens but the cursor is pinned | The engine relocked `VGUI_Surface030` and reactivated first-person input; verify surface slots 61/62/93/104 and the `LockCursor` hook. |
| x86 works and x64 does not | Wrong profile was copied, a pointer was truncated, a metadata layout was reused, or an ABI branch is missing. |
| ESP projection is mirrored/off-screen | Matrix order, coordinate convention, viewport scaling, or clip-space handling is wrong. |

Treat a symptom as a clue, not as permission to guess an offset. Add the
missing static or runtime evidence, then update the relevant aidoc.

## Reversal record template

Copy this into a new aidoc while investigating a value:

```text
Question/behavior:
Binary and module:
Architecture and Ghidra program:
Ghidra image base and module-relative address:
Semantic anchor and cross-reference:
Instruction bytes / disassembly:
Operand kind and decode formula:
Pointer basis (`this` / subobject):
Source SDK cross-check:
Config section and provenance:
Runtime match and expected diagnostic:
Plausibility/readability checks:
Known timing or build limitations:
```

## Definition of done

A reversal is ready for a tutorial feature only when it has:

- Ghidra evidence from the correct binary and a unique match where a pattern is
  used;
- a documented operand interpretation, pointer basis, and ABI/prototype;
- architecture-specific config provenance;
- checked reads/vtable calls and fail-closed behavior;
- an F1 or equivalent diagnostic that makes failure distinguishable;
- the implementation separated from scanning/resolution code;
- successful builds for every affected profile;
- a local runtime smoke test with recorded output;
- an updated numbered aidoc explaining how another person can reproduce the
  discovery.

If any of these is missing, leave the feature disabled and document what still
needs to be reversed.
