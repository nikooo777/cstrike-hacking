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
| `aidocs/001` through `aidocs/007` | The evidence-based tutorial narrative: how a value was found and why the implementation uses it. |

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
[004: x64 migration](aidocs/004_x64-migration-and-abi.md),
[005: no-spread / weapon accuracy](aidocs/005_no-spread-and-weapon-accuracy.md),
[006: Bone ESP / world-to-screen](aidocs/006_bone-esp-and-world-to-screen.md),
and [007: the menu wheel](aidocs/007_weapon-wheel-menu.md).
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
6. After a game update, do not start from scratch. The tutorial's Ghidra
   project (`/source-engine-tutorial/x64`) still holds the original file bytes
   of the last verified build. Export them from a copy of the project with
   headless Ghidra, then compare signatures, RTTI-located vtables slot by slot,
   and documented functions between the two builds with
   `tools/check_signatures.py` and `tools/compare_builds.py`. Only the
   differences need a new reversal. aidocs/004 section 11.1 records this
   procedure and its 2026-09-20 results.

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
`indirections=0`. The x64 ClientState use-site LEA likewise resolves the live
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

Avoid keeping call or jump displacements exact to make a pattern unique. The
first x64 ClientState pattern did that on a generic C++ static initializer,
and it broke on the 2026-09-20 update because both displacements moved. The
current pattern anchors on a use site (`LEA RCX,[cl]`, a call, and a compare of
the signon state with `6`) with every displacement wildcarded, and it
cross-checks with the object the `CClientState` constructor receives
(aidocs/004 section 5.1).

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
   different branch or build. MSVC RTTI (type descriptor, complete object
   locator, vtable) finds a class's vtables without a live object.
5. Validate the selected slot's function address before invoking it.

The verified slots are listed in aidocs/004 section 6, and their status on the
2026-09-20 build in section 11.1. They are evidence for particular binaries,
not universal constants.

Two lifecycle rules came out of that work:

- Cursor ownership is decided by the engine's `CEngineVGui::Simulate`, not by
  the renderer. An ImGui window is not a VGUI popup, so the menu hooks
  `VGUI_Surface030::LockCursor` instead of changing the cursor in `EndScene`
  (aidocs/004 section 6.3).
- A readable trace interface does not prove that a visibility result means
  anything. Re-check the `TraceRay` slot, the `Ray_t`/`CGameTrace` layouts, and
  the filter's entity basis together (aidocs/004 section 6.2).

## Netvars, private fields, and pointer basis

Netvars are a good source for replicated entity fields. They are not a magic
replacement for every private/client-only value:

- use the RecvTable walker for networked properties;
- locate client-only fields, caches, and helper state through code/data-flow;
- name the declaring table or function in logs and documentation;
- record whether an offset is relative to the main entity, a subobject, a
  member inside a nested object, or a pointer target.

Dormancy is not a guessed field on x64: it is queried through
`IClientNetworkable::IsDormant`. A placeholder that reports every entity as
dormant silently invalidates every target (aidocs/004 section 9.2).

### Establish the pointer basis before translating a displacement

A Ghidra displacement is relative to whatever register holds `this` in that
function, which is not necessarily the pointer the feature holds. The bone
cache is the worked example: `SetupBones` receives the renderable subobject at
`entity + 0x8`, so its `[this + 0xB40]` becomes `entity + 0xB48`
(aidocs/004 section 9.1). For any private field, write down this chain before
coding:

```text
pointer held by feature
  -> pointer passed to the discovered function
     -> register used by the disassembly
        -> field displacement shown by Ghidra
```

Then validate the resulting pointer, count, stride, and sample values with the
checked `mem::` reads. A readable address can still be the wrong object, so add
plausibility checks and fail closed. Never fix a bad read by trying
neighboring displacements until one looks nonzero.

Cached data is also timing-sensitive. `CreateMove` can run before an entity has
refreshed its bone cache, so a read-only diagnostic should report readability,
count, and usability before a feature depends on the values.

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
- diagnostics distinguish "feature disabled", "no valid target", "target data
  unreadable", and "data read successfully but application failed".

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
   checked API to the feature and keep x86/x64 differences local. New
   client-only offsets go in `sdk/client_offsets.h`.
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

## Code conventions

- **Architecture:** include `core/arch.h` and test `#if ARCH_X64()` or
  `#if ARCH_X86()`. They are function-like on purpose, so a missing include is
  a compile error instead of a silently false branch. Use `ARCH_THISCALL` for
  member-function pointer types instead of duplicating typedefs per
  architecture. Only the x86 `__fastcall` + `edx` hook shims need real
  per-architecture declarations.
- **Virtual calls:** get game vtable entries only through `mem::ReadVirtual`,
  `mem::GetVirtual<Fn>`, or `mem::HasVirtual`, which check the vtable, the slot
  overflow, and that the target is executable. Call game code under
  `__try`/`__except`.
- **SEH placement:** MSVC rejects `__try` in a function with locals that need
  destructors (C2712). Put the guarded call in a small helper, as
  `game/model.cpp` does, and keep containers in the caller.
- **Checked reads cost a system call:** every `mem::ReadBytes`/`ReadValue` does
  a `VirtualQuery` before the SEH-guarded copy. The query is deliberate,
  because it refuses guard pages that a faulting read would disarm. In
  per-frame code, read whole blocks once (`game::GetBonePositions`, the studio
  parent table) instead of one field at a time.
- **Client-only offsets:** keep every build-specific field offset and code
  shape in `sdk/client_offsets.h`, with the aidocs section that holds its
  evidence. Do not add hardcoded `DEFINE_MEMBER` fields to the entity headers
  for data nothing reads.
- **Pure logic:** keep formulas and selection tables in `game/weapon_math.*` or
  `math/`, separate from memory reads, and cover them in `tests/`. The
  `SelectPenaltyDecayRule` table exists because a bug hid in reading code once.
- **Threads:** `EndScene`, the window procedure, and `CreateMove` may run on
  different threads. Runtime toggles are `std::atomic<bool>`, and snapshots
  shared between hooks are copied under a mutex.

## No-spread and the silent camera: rules

aidocs/005 holds the model, the evidence, and the captures. The rules that
matter when changing the code:

- Do not start from an old "no-spread offset". The bullet direction is a
  pipeline (005 section 2): seed, pre-fire penalty and punch decay, two
  separate radii, the polar cone, and `+2*punch`.
- The hook runs before the engine writes `random_seed`. Derive the seed from a
  positive `command_number`. A zero-sequence command is an extra mouse sample:
  it receives no mutation, and a pending F1 dump waits for the next real
  command (005 section 4.3).
- Predict fire-time state, do not read it early. Hook-time `GetInaccuracy()` is
  one penalty-decay tick early, and punch is one movement tick early
  (005 sections 5.6 and 7.1).
- Use `weapon+0xC62` for the weapon-info lookup, not virtual weapon-id slot 371
  (005 section 5.5).
- The accuracy "branch" is the replicated `weapon_accuracy_model` ConVar
  (default 2). Model 1 is unmodeled and must fail closed (005 section 5.3).
- Compose aim, recoil, and spread once, in `game::ComposeShotAngles`. Never let
  features overwrite each other's command angles (005 section 8).
- The fire path adds `2*punch` while the camera adds `1*punch`, so command
  no-recoil must always return `false` from `CreateMove`. `silent_angles`
  decides only for other mutations. A `SetViewAngles` call inside the hook is
  overwritten by the caller (005 section 9).
- Remove visual punch read-only in `OverrideView`. Never write player punch
  around `FrameStageNotify`, whose stage 5 runs simulation (005 section 12.6).
- Judge a change by the one-shot fire-time capture, not by a wall cluster
  while natural recoil still moves the fire angle (005 sections 11.2 and 12.7).
- Keep `perfect_nospread=false` until the capture matches, and record
  observations only. A local match does not prove server acceptance.

## Rendering and Bone ESP: rules

aidocs/006 holds the evidence. Treat ESP as a pipeline, not a collection of
offsets:

```text
checked game data -> world-to-screen projection -> render-stage drawing
```

- Capture the final `CViewSetup` in `OverrideView`, after the original call and
  the visual punch correction, and consume it in `EndScene`. The overlay must be
  the full `0xC8` bytes: `GetMatricesForView` reads through `+0x88`, far past
  `angles` (006 section 3.3).
- Use the D3D viewport for screen mapping, and reject `clipW <= 0.001`
  (006 section 7).
- Take the skeleton from the studio header's parent links, never from a fixed
  bone list (006 section 6).
- Every F1 stage must stay distinguishable: `viewport`, `view`, `matrix`,
  `candidates`, `hierarchies`, `projectedLines` (006 section 10).
- Keep `EndScene` work to one pass over the player slots with block reads. Do
  not scan signatures or call `SetupBones` there. Preserve the original device
  call, and account for window-procedure restoration.
- Draw ESP on ImGui's background draw list, beneath the menu wheel.

## Menu wheel and overlay: rules

aidocs/007 holds the design and the reasoning:

- `ui/` is pure: ImGui only, with no game, hook, or config includes. The
  feature layer (`features/menu.cpp`) builds a `WheelModel` from the config
  and telemetry each frame and applies the returned click. This is what lets
  `tests/wheel_tests.cpp` and `tools/wheel_preview` exercise the real code.
- Status data reaches the menu only through `features/telemetry`: atomic
  counters, a startup record, and mutex-protected copies. Never call a
  resolver from `EndScene` for a status line, and route actions that touch
  game objects back to the game thread (the Dump item sets a flag that
  `CreateMove` consumes).
- Render a UI change with `wheel_preview` and look at every scene before
  building the DLL. The rasterizer is a review tool, not a pixel-exact
  reproduction of the GPU.
- Avoid Win32 macro names in any file that can see `Windows.h`: `DrawText`,
  `near`, `far`, `CreateWindow`, `LoadImage`, `GetObject`, and `min`/`max`
  unless `NOMINMAX` comes first (as in `memory/mem.cpp`). A Linux-only build
  of the pure code will not catch them.
- ImGui's DX9 objects live in `D3DPOOL_DEFAULT` and must be released before a
  device reset. The `Reset` and `ResetEx` hooks do this; any new
  `D3DPOOL_DEFAULT` resource must be released there too (007 section 7.1).
- The embedded fonts are under the SIL Open Font License; keep
  `ui/fonts/OFL.txt` beside them. MSVC caps a concatenated string literal at
  65,535 bytes, so a larger font must be split into several arrays.

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
| The game cannot recover after a resolution change | A `D3DPOOL_DEFAULT` resource outlived the reset, or the device's `Reset`/`ResetEx` entry is not the hooked one; check the startup log (aidocs/007 section 7.1). |
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
