# 004 - Porting the experiment from x86 to x64

This chapter records the first x64 pass against the updated Counter-Strike:
Source client. It covers what changes when the pointer size changes, how each
x86 result was re-found in the x64 binaries, and which ABI details are easy to
get subtly wrong. It is tied to the binaries that were on disk at the time,
not a claim that one set of offsets survives every update. Section 11
records one that did not.

Use this only with binaries and processes you are permitted to study,
preferably in an offline or local test environment.

## What changes between x86 and x64

| Topic | x86 build | x64 build |
| --- | --- | --- |
| Pointers, `std::uintptr_t`, `size_t`, vtable entries | 4 bytes | 8 bytes |
| `int`, `DWORD`, `float`, `long` | 4 bytes | 4 bytes (unchanged) |
| Global access in code | absolute `imm32` address | signed `rel32` relative to the next instruction |
| Member calls | `__thiscall`, hooked through a `__fastcall` + `edx` detour | one Microsoft x64 convention (`RCX`, `RDX`, `R8`, `R9`) |
| `ClientClass` / `RecvTable` / `RecvProp` pointers | 4 bytes | 8 bytes, which moves every later field |
| MinHook | prebuilt x86 library | built from the vendored source for the target ABI |

Do not turn every field into a 64-bit value. Only pointer fields and
pointer-sized intermediates change width. Pointer fields do add padding,
though, so later fields in the same structure can move. That is why the same
netvar or client-only field often has a different offset in each build.

## Binaries used for this chapter

The target modules are split between the game directory and architecture
subdirectories:

```text
cstrike/bin/x64/client.dll
cstrike/bin/x64/server.dll
bin/x64/engine.dll
bin/x64/vgui2.dll
bin/x64/vguimatsurface.dll
```

The process still exposes them by the familiar names (`client.dll`,
`engine.dll`, and so on), so runtime module names do not change. The DLL must,
however, be built x64 and loaded into the x64 process.

Unless a section says otherwise, the module-relative addresses below describe
the August 2026 x64 client that this tutorial was developed against. Only the
`vguimatsurface.dll` hashes were recorded at the time. The other August hashes
were recovered later from the Ghidra project (section 11.1). Record the SHA-256
of every module you reverse: an address without a binary identity quickly
becomes ambiguous.

## 1. Start with the PE headers

Before opening Ghidra, confirm that the files really belong to different
architectures. The old client reports:

```text
Machine: 0x014C
Optional header: 0x010B
```

The updated client reports:

```text
Machine: 0x8664
Optional header: 0x020B
```

`0x8664` is the PE x64 machine type and `0x020B` is the PE32+ optional
header. This check prevents a common mistake: changing CMake to x64 while
continuing to inspect or launch the old 32-bit module set.

## 2. Import the x64 modules into Ghidra

Create a separate Ghidra project, or at least separate program imports. Do not
reuse the old x86 program's addresses, data types, or structure overlays
because a symbol or interface has the same name. For this pass the useful
programs were:

| Program | Ghidra path | Image base | Language |
| --- | --- | ---: | --- |
| `client.dll` | `/source-engine-tutorial/x64/client.dll` | `0x180000000` | `x86:LE:64:default` |
| `engine.dll` | `/source-engine-tutorial/x64/engine.dll.0` | `0x180000000` | `x86:LE:64:default` |

Keep module-relative addresses in notes. ASLR changes the loaded base, while a
match such as `client.dll+0x1F1443` is directly comparable with the runtime
log:

```text
runtime address = loaded module base + module-relative offset
```

## 3. Replace absolute operands with RIP-relative decoding

The old x86 ClientMode pattern begins with:

```asm
MOV ECX, dword ptr [global]
```

The four-byte absolute address is embedded immediately after the opcode. In
x64, the same kind of global access is commonly encoded as:

```asm
LEA RBX, [RIP + rel32]
```

The target is calculated from the address immediately after the instruction:

```text
target = instruction_address + instruction_length + sign_extend(rel32)
```

It is not correct to read the four bytes as a complete pointer. The
`mem::DecodeRipRelative32` helper reads a signed 32-bit displacement and adds
it to the end of the instruction. Because a signature can start with setup
bytes before the RIP-relative instruction, the configuration records three
separate numbers:

| Config key | Meaning |
| --- | --- |
| `operand=rip_rel32` | Select RIP-relative decoding (`abs32` is the x86 form). |
| `operand_offset` | Offset of the four displacement bytes inside the match. |
| `instruction_offset` | Offset of the start of the RIP-relative instruction inside the match. |
| `instruction_length` | Length used in the formula above. |
| `indirections` | Extra pointer reads after decoding. |

Choose `indirections` from the instruction's meaning, not from intuition:

```text
LEA RBX, [RIP + rel32]   -> decoded value is the object address; usually indirections=0
MOV RCX, [RIP + rel32]   -> decoded value is a global slot; read it once to get the object
```

This keeps the scanner architecture-neutral while the binary-specific evidence
stays in the configuration file.

## 4. Re-find ClientMode in the x64 client

In Ghidra, the x64 ClientMode evidence is:

1. RTTI identifies `ClientModeShared`.
2. The resolved CS client-mode object installs its final vtable at
   `client.dll+0x477638`. `client.dll+0x42DF88` is the base
   `ClientModeShared` vtable seen during construction, not the final table.
3. `OverrideView` is slot 16 (`client.dll+0xE67C0`) and `CreateMove` is slot
   21 (`client.dll+0xE5290`), matching
   `source-sdk-2013/src/game/client/iclientmode.h`.
4. The initialization/accessor sequence at `client.dll+0x1F1443` starts with
   `LEA RBX, [global]` and resolves the live object at `client.dll+0x68D4A0`.

The Ghidra byte search for the following pattern returned exactly one match,
at `0x1801F1443`:

```text
48 8D 1D ?? ?? ?? ??
48 8B CB
89 05 ?? ?? ?? ??
E8 ?? ?? ?? ??
48 8B 0D ?? ?? ?? ??
48 8D 05 ?? ?? ?? ??
48 89 05 ?? ?? ?? ??
```

The config representation in `config/signatures-x64.ini` keeps the pattern on
one line and decodes the first instruction:

```ini
operand=rip_rel32
operand_offset=3
instruction_offset=0
instruction_length=7
indirections=0
```

The first RIP-relative operand points directly at the object, so there is no
second pointer read. The x86 profile is different: its absolute operand points
at a global slot containing `ClientMode*`, so `indirections=1` is correct
there.

## 5. Re-find ClientState in the x64 engine

The x64 engine contains the `CClientState` RTTI and a constructor at
`engine.dll+0xA0DF0`. Its static initialization path at `engine.dll+0x47E0`
passes the live object at `engine.dll+0x535810` to that constructor.

A short wildcarded initializer pattern is ambiguous: many C++ static
initializers have the same stack adjustment, LEA, call, cleanup, and jump
shape. Ghidra returned many matches for the fully wildcarded form. Keeping the
verified call and tail-jump bytes exact made the pattern unique:

```text
48 83 EC 28 48 8D 0D ?? ?? ?? ??
E8 00 C6 09 00
48 8D 0D ?? ?? ?? ?? 48 83 C4 28
E9 6C F7 30 00
```

The exact match check returned only `0x1800047E0`. Its first LEA begins four
bytes into the signature, so it uses `operand_offset=7`,
`instruction_offset=4`, and `instruction_length=7`. It resolves directly to
the ClientState object (`indirections=0`).

Keeping the rel32 bytes exact was a deliberate trade-off for one build. Those
bytes encode the distance to the called function, so they change whenever
either side of the call moves. The 2026-09-20 update did exactly that, and the
pattern stopped matching. Treat any pattern with exact displacement bytes as
the first thing to re-check after an update.

### 5.1 Re-deriving ClientState after the 2026-09-20 update

The re-derivation followed the same semantic chain, then looked for a better
anchor than the initializer:

1. The `.?AVCClientState@@` RTTI type descriptor leads to four complete object
   locators (offsets `0x0`, `0x8`, `0x10`, and `0x8C00`, one per base
   subobject) and their four vtables.
2. The only function that installs all four vtables in a row is the
   constructor at `engine.dll+0xA0E70` (August: `+0xA0DF0`). It calls the base
   constructor, builds the subobject at `this + 0x8C00`, and then writes the
   vtables at `this`, `+0x8`, `+0x10`, and `+0x8C00`.
3. The constructor has one caller, the static initializer at `engine.dll+0x47E0`,
   unchanged from August. Its first LEA passes the live object at
   `engine.dll+0x536810` (August: `+0x535810`). Only the call and tail-jump
   displacements differ from the old pattern: `00 C6 09 00` became
   `80 C6 09 00`, and `6C F7 30 00` became `AC 06 31 00`.

Instead of pinning the new displacements, the new pattern anchors on a **use
site** of the object, with every displacement wildcarded:

```text
48 8D 0D ?? ?? ?? ??     LEA  RCX, [ClientState]
E8 ?? ?? ?? ??           CALL <ClientState method>
83 3D ?? ?? ?? ?? 06     CMP  dword ptr [ClientState + 0x14C], 6
```

It matches exactly once, at `engine.dll+0x8CE1B`. The site calls a method on
the object and then compares the field at `+0x14C` with `6`, which is
`SIGNONSTATE_FULL` in the Source SDK. The next instruction loads the pointer
at `object + 0x20`, where the x64 `CBaseClientState` layout places
`m_NetChannel` (after three interface vtable pointers and `m_Socket`). The
fixed bytes are opcodes, the register choice, and one meaningful constant,
none of which move when code is relocated.

The decode is `operand_offset=3`, `instruction_offset=0`,
`instruction_length=7`, `indirections=0`. It resolves `engine.dll+0x536810`,
the same object the initializer passes to the constructor. The use site and the
constructor path therefore cross-validate each other. The same pattern also
matches the recovered August engine exactly once (`+0x8CD9B`) and resolves the
August object `+0x535810`. It works on both builds, which the initializer
pattern did not (section 11.1).

The pattern ends before the `JNZ` that follows the compare. A short conditional
jump becomes a long one when its target moves more than 127 bytes away, which
would change the pattern's shape.

## 6. Verify interfaces and vtable slots instead of copying x86 addresses

The x64 client still registers the semantic interfaces:

```text
VClient017
VClientEntityList003
VEngineClient014
```

Ghidra shows the x64 `InterfaceReg` record as three eight-byte fields:

```text
offset 0x00: create function pointer
offset 0x08: interface-name pointer
offset 0x10: next-record pointer
```

The workflow for each interface is the same: search the correct module for
the interface string, follow its reference to the registration record and
create function, identify the returned object, then read its vtable in the
actual binary. The Source SDK header is a cross-check for method order, not
proof for a different branch or build. Validate the selected slot's function
address as readable and executable before invoking it.

The x64 `VClient017` object is at `client.dll+0x5AE6D8`. The
`VClientEntityList003` object is at `client.dll+0x6498C8`. The pointer width
changes, but each slot below was checked against the loaded vtable rather than
inferred from the old object address.

Slots verified for the sample x64 build, counting from zero:

| Object/interface | Method | Slot | Details |
| --- | --- | ---: | --- |
| `VClient017` | `GetAllClasses` | 8 | chapter 002 and section 8 |
| `VClient017` | `IN_ActivateMouse` / `IN_DeactivateMouse` | 14 / 15 | section 6.3 |
| `VClient017` | `FrameStageNotify` | 35 | chapter 005 |
| `VClientEntityList003` | `GetClientEntity` | 3 | chapter 003 |
| `VClientEntityList003` | `GetClientEntityFromHandle` | 4 | section 7.1 |
| `ClientMode` | `OverrideView` / `CreateMove` | 16 / 21 | section 4 |
| `VEngineClient014` | `GetViewAngles` / `SetViewAngles` | 19 / 20 | section 6.1 |
| `EngineTraceClient003` | `TraceRay` | 4 | section 6.2 |
| `IClientUnknown` | `GetClientNetworkable` | 4 | section 9.2 |
| `IClientNetworkable` | `IsDormant` | 8 | section 9.2 |
| `IClientRenderable` | `GetModel` | 9 | chapter 006 |
| `C_CSPlayer` | fire-angle source (diagnostic only) | 143 | chapter 005, section 11 |
| `VEngineRenderView014` | `GetMatricesForView` | 50 | chapter 006 |
| `VModelInfoClient006` | `GetStudiomodel` | 28 | chapter 006 |
| `VGUI_Surface030` | `SetCursor` | 51 | section 6.3 |
| `VGUI_Surface030` | `UnlockCursor` / `LockCursor` | 61 / 62 | section 6.3 |
| `VGUI_Surface030` | `CalculateMouseVisible` / `IsCursorLocked` | 93 / 104 | section 6.3 |
| `IDirect3DDevice9` | `EndScene` | 42 | D3D9 ABI |

A slot number can stay the same while the vtable address and every entry
change. These are evidence for the sample build, not universal constants.

### 6.1 View angles through `VEngineClient014`

The engine view-angle path is a good example of preferring a semantic
interface over a guessed structure overlay. The current x64 engine registers
`VEngineClient014`. The Source SDK declaration places `GetViewAngles(QAngle&)`
at vtable slot 19, with `SetViewAngles` at slot 20.

In Ghidra, the x64 interface object is returned by the factory at
`engine.dll+0x71440`, whose LEA resolves `engine.dll+0x4605A0`. Its vtable is
at `engine.dll+0x366070`. Slot 19 points to `engine.dll+0x70630`, and that
function copies three floats from `engine.dll+0x53E4E4`, `+0x53E4E8`, and
`+0x53E4EC` into the caller's `QAngle` buffer. Chapter 005 section 9 lists
both slots' listings.

The implementation calls slot 19 through the configured `VEngineClient014`
interface. No x64 `ClientState` view-angle displacement is assumed. Do not
reuse the old x86 `ClientState + 0x4B84` overlay on x64.

### 6.2 Visibility through `EngineTraceClient003`

The aimbot needed a semantic visibility check rather than a guessed "visible"
field. The x64 `engine.dll` program contains the string
`EngineTraceClient003` at `engine.dll+0x3A41E0`. Its reference at
`engine.dll+0xCB70` loads the interface name and jumps to the common factory
path at `engine.dll+0x2803F0`. This confirms that the engine registers the
client trace interface. The method order still has to be checked against the
loaded vtable.

The matching Source SDK `IEngineTrace` declaration is the ABI cross-check:

```text
slot 0  GetPointContents
slot 1  GetPointContents_Collideable
slot 2  ClipRayToEntity
slot 3  ClipRayToCollideable
slot 4  TraceRay(const Ray_t&, unsigned int, ITraceFilter*, trace_t*)
```

The implementation resolves `EngineTraceClient003` by name and validates slot
4 as an executable address before calling it. Its local ABI view asserts the
Source layouts used by the call:

- `Ray_t` is `0x50` bytes, with its two boolean flags at `+0x40` and `+0x41`;
- `CGameTrace::fraction` is at `+0x2C`, and the full trace object is declared
  so the engine can write its later fields;
- the x86 function pointer uses `__thiscall`, and x64 uses the normal
  Microsoft x64 member-call ABI.

Visibility uses Source's `MASK_VISIBLE` (`0x6081`) and an `ITraceFilter` that
skips both the local entity and the candidate. A trace is considered clear
only when `fraction >= 0.999`. A failed interface lookup, invalid vtable entry,
exception, malformed fraction, unreadable bone cache, or non-finite point
returns false instead of letting the aimbot guess.

Target selection iterates the complete player range (`1` through
`MAXPLAYERS - 1`), reads the head bone before ranking, rejects blocked
targets, and compares eye-to-head distance. The selection is rebuilt on every
command, so there is no stale target pointer to clear when a player dies. A
candidate whose bone cache is unavailable is skipped instead of aborting the
whole search. This is a client-side line-of-sight approximation, not proof of
server visibility. Re-verify the interface, slot, filter entity basis, and
trace layouts together after an engine update; a readable interface alone does
not prove that a visibility result is meaningful.

### 6.3 Cursor ownership is a VGUI lifecycle problem

Changing the Win32 cursor from `EndScene` is not enough to make an ImGui menu
interactive. ImGui is not a native VGUI popup, so the engine's UI simulation
still concludes that no UI needs the mouse. On a later frame it locks the
cursor and reactivates first-person mouse input. That explains why the arrow
can be visible while staying pinned to the center of the game window.

The exact `vguimatsurface.dll` binaries used for this pass are:

| Architecture | SHA-256 |
| --- | --- |
| x64 | `47B534C9C354F3600A95B9E97750EF84F6DC46AACD9CF338A655E4D3D0D08B59` |
| x86 | `71227A6CC7D8AF52D5A6B4468633F96E4AFB74E80AA6D2252E49DB48E094AA0C` |

Following `VGUI_Surface030` to its returned object and vtable in Ghidra gives
the same method order in both binaries:

| Method | Slot | x64 module offset | x86 module offset |
| --- | ---: | ---: | ---: |
| `SetCursor` | 51 | `+0x12DC0` | `+0x44F80` |
| `UnlockCursor` | 61 | `+0x13990` | `+0x45AB0` |
| `LockCursor` | 62 | `+0x11040` | `+0x433F0` |
| `CalculateMouseVisible` | 93 | `+0x9090` | `+0x3CD20` |
| `IsCursorLocked` | 104 | `+0x10F10` | `+0x432A0` |

The x64 surface vtable is at `vguimatsurface.dll+0xC83C0` and the x86 vtable is
at `vguimatsurface.dll+0xD4B08`. The x64 slot-61 stub clears `ECX` before
jumping to the common lock-state function, while slot 62 sets `CL` to one. The
x86 stubs push zero and one respectively before calling their common
implementation. These instruction-level differences confirm that the slots
are unlock and lock, instead of relying only on SDK method order.

Ghidra also confirms the caller in the matching x64 `engine.dll`.
`CEngineVGui::Simulate` is at `engine.dll+0x227620`:

```text
+0x227848  call surface slot 93  (CalculateMouseVisible)
+0x22787E  check surface slot 104 (IsCursorLocked)
+0x22789B  locked   -> VClient017 slot 14 (IN_ActivateMouse)
+0x2278AA  unlocked -> VClient017 slot 15 (IN_DeactivateMouse)
```

The matching Source SDK follows the same `CalculateMouseVisible` then
`VGui_ActivateMouse` flow.

The implementation therefore hooks `VGUI_Surface030::LockCursor` instead of
fixing the cursor in the render hook. With the ImGui menu closed it calls the
original method. With the menu open it calls verified slot 61 instead and
selects the arrow through slot 51. The engine's following `IsCursorLocked`
check then deactivates mouse input through its own normal path. Closing the
menu restores the original lock and activation transition. Unloading disables
the hook before restoring ImGui and the window procedure.

The x64 runtime smoke test confirmed the complete transition: pressing
**Insert** directly from gameplay released the pointer, made the ImGui controls
clickable without first opening another game UI, and returned input ownership
to Source when the menu closed.

## 7. Fix the hook ABI

On x86, the detours use an x86-compatible shape:

```cpp
bool __fastcall hook(void *thisPtr, void *edx,
                     float sampleTime, CUserCmd *cmd);
```

The extra `edx` parameter is the usual way to bridge a `__thiscall` target
with a `__fastcall` detour. MSVC x64 uses one calling convention and does not
pass that placeholder. The x64 declarations therefore use:

```cpp
bool hook(void *thisPtr, float sampleTime, CUserCmd *cmd);
```

The project keeps the x86 declarations under `_M_IX86` and the x64
declarations under the other branch. `__thiscall`, `__fastcall`, and
`__stdcall` are not portable annotations when reviewing an x64 hook
signature. Compare the actual parameter list and register ABI. Before
installing a hook, match:

- the return type and its width;
- parameter order, and whether each parameter is a pointer, integer, float,
  or reference;
- whether the target is a member call and where `this` arrives;
- the vtable slot and the actual function's prologue and call sites.

The same split is used by the verified `OverrideView(CViewSetup *)` hook at
ClientMode slot 16. Its compile-time layout checks require
`CViewSetup::origin == 0x40` and `CViewSetup::angles == 0x4C` on both builds.
Chapter 006 extends that overlay to the full `0xC8` bytes needed by the matrix
builder.

A prototype mismatch shows up differently in each build. On x86, an MSVC error
such as "the value of ESP was not properly saved" usually means a wrong calling
convention or prototype, and the line named by the runtime check may only be
where the corrupted stack is noticed. On x64 it tends to show up as bad
register values, corrupted home space, or a crash after the hook returns.

### 7.1 Small non-trivial by-value parameters: `CBaseHandle`

A parameter's size alone does not decide how it is passed. The first
implementation declared the handle-taking entity-list virtuals as accepting
`std::uint32_t`, because the stored `m_hActiveWeapon` value is four bytes. That
is wrong on MSVC x64.

In the matching Source SDK, `CBaseHandle` is a four-byte class with
user-defined constructors and copy operations, and
`GetClientEntityFromHandle(CBaseHandle)` takes it **by value**. On MSVC x64 the
caller passes a class like that as the address of a temporary object, not as
the integer itself. Ghidra confirms the distinction in the current x64 binary:

| Evidence | Address |
| --- | ---: |
| `VClientEntityList003` string | `client.dll+0x42D770` |
| interface object / vtable path | `client.dll+0x6498C8` / `+0x42D7B0` |
| `GetClientEntityFromHandle`, slot 4 | `client.dll+0xDF700` |

The slot-4 listing begins:

```asm
MOV EAX, dword ptr [RDX]       ; read CBaseHandle::m_Index through the pointer
LEA RDX, [RSP + 0x30]          ; build the next by-value temporary
MOV dword ptr [RSP + 0x30], EAX
CALL qword ptr [RAX + 0x10]    ; GetClientUnknownFromHandle
```

`RDX` points at a `CBaseHandle` temporary. The SDK-shaped declaration in
`sdk/client_entity_list.h` mirrors the real non-trivial class and asserts its
four-byte size, which preserves that ABI. The integer declaration failed
quietly: the handle was read successfully while every lookup returned null. A
log such as `handle=0x01390146 index=0x146` therefore proves only that the
field read worked. The resolution has succeeded only once `weapon=0x...` and
`methodsOk=yes` are reported (chapter 005, section 11).

## 8. Update the metadata ABI

The netvar walker reads game-owned `ClientClass`, `RecvTable`, and `RecvProp`
objects. Pointer fields cannot be represented by `DWORD` in x64. The current
declarations use pointer types, and compile-time assertions document the
expected offsets:

| Field | x86 | x64 |
| --- | ---: | ---: |
| `ClientClass::recvTable` | `0x0C` | `0x18` |
| `RecvProp::offset` | `0x2C` | `0x48` |
| `RecvTable::name` | `0x0C` | `0x18` |

These are offsets inside the metadata structures themselves, not entity
offsets. If one assertion fails, stop and re-check the ABI declaration instead
of compensating with a different netvar path.

Likewise, an entity field that stores a pointer must use `std::uintptr_t` or
another pointer type. The bone-matrix member is one such field. A 32-bit
`DWORD` would truncate the x64 address even if the field displacement happened
to stay the same.

## 9. Client-only fields: establish the pointer basis first

Netvars cover replicated properties only. Caches, helper state, and fields the
server never sends have to be found through code and data flow. Those
listings show displacements from whatever register holds `this` in that
function, and that register is not always the pointer the feature holds. In
particular, a pointer returned by `GetClientEntity` is not necessarily the
pointer passed to every virtual method. With multiple inheritance, a method on
an embedded interface subobject receives an adjusted `this`.

Before coding any client-only field, write down this chain:

```text
pointer held by the feature
  -> pointer passed to the discovered function
     -> register used by the disassembly
        -> field displacement shown by Ghidra
```

Then validate the resulting pointer, count, stride, and sample values with
`mem::IsReadable` and `mem::ReadValue`. A readable address can still be the
wrong object, so add plausibility checks and fail closed. Never "fix" a bad
read by trying neighboring displacements until one looks nonzero.

### 9.1 Bone cache through `C_BaseAnimating::SetupBones`

The old x86 overlay used `entity + 0x578` as the cached bone-matrix pointer.
That value cannot be carried into x64 out of habit. In the x64 client, search
for the `C_BaseAnimating::SetupBones` string and follow its reference. The
current Ghidra program identifies the function at `client.dll+0x7B4A0`.

The cached-copy path is the useful evidence:

```asm
MOVSXD R8, dword ptr [R14 + 0xB50] ; cached bone count
MOV    RDX, qword ptr [R14 + 0xB40] ; m_CachedBoneData.Base()
...
CALL   FUN_1803D6520              ; copy count * 0x30 bytes
```

The important ABI detail is what `R14` holds. `SetupBones` is dispatched
through `IClientRenderable`, so its `this` pointer is the renderable
subobject, eight bytes after the `IClientEntity` pointer returned by
`GetClientEntity`. The nearby `R14 - 8` expressions recover the main entity
object and its vtable. The raw Ghidra displacements must therefore be
translated by `+0x8` when the code starts from the entity-list pointer:

| Value | `SetupBones` `this` (renderable) | Entity-list pointer |
| --- | ---: | ---: |
| cached bone matrix pointer | `+0xB40` | `+0xB48` |
| cached bone count | `+0xB50` | `+0xB58` |

The matrix stride is `0x30` bytes (a 3x4 float matrix). Bone 14's position is
at matrix offsets `+0x0C`, `+0x1C`, and `+0x2C`.

The implementation reads the translated `entity + 0xB48` and `entity + 0xB58`
fields. The F1 dump prints the raw pointer, count, and readability flags even
when the cache is not populated. A zero pointer or count indicates that the
cache has not been filled when `CreateMove` runs, while a nonzero, plausible
pair confirms the layout. Bone data is timing-sensitive: `CreateMove` can run
before an entity has refreshed its cache, so the diagnostic separates
"readable" from "usable".

The x64 runtime pass confirmed the translation. The F1 dump reported
`matrixReadable=yes`, `countReadable=yes`, `usable=yes`, and a bone count of
`50` for the local player. Absolute entity and matrix addresses are omitted
here because ASLR changes them between launches.

### 9.2 Dormancy through `IClientNetworkable`

A valid bone cache is not enough for target selection. The first x64 profile
deliberately set `aimbot=false`, and its placeholder `m_bDormant()` returned
`true` for every entity, so `IsValidTarget()` rejected all players by design.

The x86 dormant member is not reused on x64. For this build the semantic path
is the client networkable interface. The x64 entity-list wrapper at
`client.dll+0xDF750` reaches the entity's
`IClientUnknown::GetClientNetworkable` entry at vtable slot 4. The Source SDK
declaration places `IClientNetworkable::IsDormant` at slot 8. The runtime
validates both vtable function addresses, calls those two methods, and fails
closed if either interface cannot be resolved.

With that path in place the x64 profile enables the aimbot again. The F1 dump
reports the resolver state plus the number of valid targets and readable
bone-14 matrices, which separates target filtering from angle application
during testing.

### 9.3 Crosshair target through `C_CSPlayer::GetIDTarget`

The crosshair target is client-only in both builds. Neither exposes it as a
receive-table property, so there is no `m_iCrosshairID` netvar to resolve.
Semantic tracing from `CTargetID` to `C_CSPlayer::GetIDTarget` identifies the
x64 field at `player + 0x1B20`. Ghidra shows the accessor reading
`[RDI + 0x1B20]` at `client.dll+0x1EC456`, and the update path writing the same
displacement. The x86 field is `player + 0x14F0` (chapter 002).

The field is mapped, but the x64 profile still ships with `triggerbot=false`.
The rest of the triggerbot's input and target path has not been validated at
runtime on x64.

## 10. Build the correct profile

The CMake file derives the architecture from pointer size:

```text
CMAKE_SIZEOF_VOID_P == 4  -> x86 profile + config/signatures.ini
CMAKE_SIZEOF_VOID_P == 8  -> x64 profile + config/signatures-x64.ini
```

MinHook is built from the [official MinHook source repository](https://github.com/TsudaKageyu/minhook),
vendored under the project for the selected architecture, rather than linking
only the old prebuilt x86 libraries. The relevant source and license are in
`minhook/src`, `minhook/include`, and `minhook/LICENSE.txt`.

From an x64 Native Tools command prompt:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -S . -B build-msvc-x64 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msvc-x64 --target nikooo777
```

The DirectX SDK path can still be overridden with `-DDXSDK_DIR=...`. CMake
selects `Lib/x64/d3d9.lib` for this profile and copies the selected
signature profile to `build-msvc-x64/signatures.ini`, next to the DLL. Edit the
source profile under `config/`, not the copy. Do not reuse an x86 build
directory when changing pointer size.

## 11. Status

For the August 2026 x64 build:

| Item | Status | Evidence |
| --- | --- | --- |
| Architecture-selected build and profile | Done | section 10 |
| RIP-relative operand decoding | Done | ClientMode and ClientState resolve uniquely (sections 4 and 5) |
| Interfaces and vtable slots | Verified | section 6 table |
| View angles | `VEngineClient014` slot 19; no ClientState overlay | section 6.1 |
| Visibility | `TraceRay` slot 4 with asserted `Ray_t`/`CGameTrace` layouts | section 6.2 |
| Menu cursor ownership | Runtime smoke test passed | section 6.3 |
| Hook prototypes and `CBaseHandle` ABI | Verified against listings | section 7 |
| Netvar metadata ABI | Compile-time assertions | section 8 |
| Bone cache | Runtime: count `50`, readable and usable | section 9.1 |
| Dormancy | Interface path; x64 aimbot enabled | section 9.2 |
| Crosshair target | Field mapped at `player + 0x1B20`; triggerbot still off | section 9.3 |

A successful DLL build and successful interface resolution do not validate
every client-only field. Keep a feature disabled until its complete path has
static evidence and a runtime check.

### 11.1 The 2026-09-20 update

The x64 game modules were updated on 2026-09-20. The update was checked
statically, without loading the DLL, by comparing the new binaries against the
exact August binaries the tutorial was reversed against.

| Module | August SHA-256 | 2026-09-20 SHA-256 |
| --- | --- | --- |
| `cstrike/bin/x64/client.dll` | `D74AAC886111EA47484E0B4CEF6A18F9BB048695C554FF706CFF17DF331F96CC` | `F561044F6926DC1E9A2E3402F122B24274671C55962C8915E35A0A1C18A009C4` |
| `bin/x64/engine.dll` | `536EAB44AED8EA4F25DA2CBC090570BCEB89AB05A7DC2CD65AF8B97A296E6771` | `5F611B84FAC362E7F805289F849B4D427B60F59539706F174E2B9353B8F0E8F9` |
| `bin/x64/vguimatsurface.dll` | `47B534C9C354F3600A95B9E97750EF84F6DC46AACD9CF338A655E4D3D0D08B59` | `F6A652BB1CBD9D74D699061FA0BAFAB69F7C0219BDDDEE9A4FC0989FBAA45D42` |
| `cstrike/bin/x64/server.dll` | not recovered | `B42EDB61CFA13AEF6397EB0278FB38656F1EB71E7AC306F00343C671AB2A6CAE` |

#### Recovering the old binaries

Steam replaces the game files on update, but a Ghidra program keeps the
original file bytes of every import. The August modules were recovered from the
tutorial's Ghidra project (`/source-engine-tutorial/x64`):

1. Copy the project's `.gpr`, `project.prp`, index, and the needed program
   databases to a scratch directory, leaving the original and its lock alone.
   Change the owner in the copy's `project.prp` if the project was created on
   another machine or account.
2. Run `analyzeHeadless` on the copy with `-readOnly -noanalysis` and
   `tools/ghidra/ExportFileBytes.java`, which writes `Memory.getAllFileBytes()`
   back out through `FileBytes.getOriginalBytes`:

   ```sh
   analyzeHeadless <copy> css/source-engine-tutorial/x64 -process client.dll \
       -readOnly -noanalysis -scriptPath tools/ghidra \
       -postScript ExportFileBytes.java <output dir>
   ```

The comparisons below are scripted in `tools/`:

```sh
tools/check_signatures.py config/signatures-x64.ini <install or DLL dir>
tools/compare_builds.py <old DLL dir> <new install> --locate client.dll:0x152290
```

`check_signatures.py` reports each pattern's match count and decoded target.
`compare_builds.py` compares every vtable slot the DLL calls between two builds
and relocates any `--locate` function. Both exit non-zero when something needs
a new reversal.

The recovered `vguimatsurface.dll` hashes to the value recorded in section 6.3,
which confirms that these are the tutorial's binaries.

#### Signatures

| Signature | Module | August match | 2026-09-20 match |
| --- | --- | ---: | ---: |
| `ClientState` (old initializer pattern) | `engine.dll` | `+0x47E0` | **no match** |
| `ClientState` (use-site pattern, section 5.1) | `engine.dll` | `+0x8CD9B` -> `+0x535810` | `+0x8CE1B` -> `+0x536810` |
| `ClientMode` | `client.dll` | `+0x1F1443` -> `+0x68D4A0` | `+0x1F0613` -> `+0x68CF80` |
| `WeaponInfoLookup` | `client.dll` | `+0x1DC630` | `+0x1DB800` |
| `ClientFireBullets` | `client.dll` | `+0x236C90` -> `+0x1FFA30` | `+0x235E80` -> `+0x1FEC00` |
| `GlobalVars` | `client.dll` | `+0x2368E4` -> `+0x5AE280` | `+0x235AD4` -> `+0x5AE280` |
| `UpdateAccuracyPenalty` | `client.dll` | `+0x2367D0` | `+0x2359C0` |

Every wildcarded pattern matched exactly once in both builds. Code moved by
about `-0xE10` to `-0xE30`, which the resolver absorbs without any change. Only
the old `ClientState` pattern failed, because it kept its call and tail-jump
displacements exact. `ClientState` is `required=true`, so that one pattern
stopped the x64 DLL at startup with `ClientState signature not found`. The
use-site pattern from section 5.1 replaces it, and it also matches the August
engine, where it resolves the documented August object.

#### Vtables

Each class's vtables were found in both builds through MSVC RTTI (type
descriptor, complete object locator, vtable). Every slot's function was then
compared after masking call, jump, and RIP-relative displacements:

| Class and subobject | Slots (Aug / new) | Slots used by the DLL | Result |
| --- | ---: | --- | --- |
| `CEngineClient` | 146 / 146 | 19, 20 | all 146 slots unchanged |
| `CVRenderView` | 52 / 52 | 50 | all unchanged; slot 50 still at `engine.dll+0x12EBE0` |
| `CModelInfoClient` | 61 / 61 | 28 | all unchanged |
| `CEngineTraceClient` | 23 / 23 | 4 | all unchanged |
| `CMatSystemSurface` | 181 / 181 | 51, 61, 62, 93, 104 | used slots unchanged, at the same addresses; 5 other slots changed |
| `CHLClient` | 79 / 79 | 8, 14, 15, 35 | used slots unchanged; 2 other slots changed |
| `CClientEntityList` + `0x40028` | 9 / 9 | 3, 4 | all unchanged |
| `ClientModeCSNormal` | 54 / 54 | 16, 21 | used slots unchanged; 8 other slots changed |
| `C_CSPlayer` (`+0x0`, `+0x8`, `+0x10`) | 303 / 45 / 14 in both | 4, 143, 9, 8 | used slots unchanged |
| `C_WeaponCSBaseGun` / `C_AK47` | 386 / 386 | 370, 371, 382, 383, 384 | used slots unchanged |
| `CCSGameMovement` | 56 / 56 | 15 | used slot unchanged |

The functions behind the used slots also agree on at least 98% of their masked
first 256 bytes (512 for `TraceRay`), not only at their entries. The weapon
slots were checked on both `C_WeaponCSBaseGun` and the live AK's `C_AK47`
vtable. The cursor stubs keep their meaning: slot 61
is still `XOR ECX,ECX` then a jump to the shared lock-state function, and slot
62 is still `MOV CL,1` then the same jump. The AK's slot 382 still begins with
the `weapon_accuracy_model` load and compare that `game::ReadAccuracyModel`
checks (chapter 005, section 5.3).

For the interfaces, the registration path from section 6 leads to the same
vtables:

| Interface | 2026-09-20 thunk | Creator | Object | Vtable |
| --- | ---: | ---: | ---: | ---: |
| `VEngineRenderView014` | `engine.dll+0x9280` | `+0x12F210` | `+0x4723D8` | `+0x394460` |
| `VModelInfoClient006` | `engine.dll+0x105E0` | `+0x1CBE30` | `+0x47BF30` | `+0x3AFA28` |
| `VEngineClient014` | `engine.dll+0x3950` | `+0x714C0` | `+0x4615A0` | `+0x367080` |
| `EngineTraceClient003` | `engine.dll+0xCBF0` | `+0x191370` | `+0x47A6F0` | `+0x3A5370` |
| `VGUI_Surface030` | `vguimatsurface.dll+0x1060` | `+0x139A0` | `+0x1146F0` | `+0xC93C8` (set at runtime) |
| `VClient017` | `client.dll+0xB8C0` | `+0xD7B30` | `+0x5AE6D8` | `+0x42C2B0` |
| `VClientEntityList003` | `client.dll+0xC580` | `+0xDFDB0` | `+0x6498F8` | set at runtime |

The August chains reproduce the addresses recorded in sections 6 and 6.1 and in
chapter 006.

`CEngineVGui::Simulate` is now `engine.dll+0x227700` (August `+0x227620`,
`CEngineVGui` slot 35). It still calls surface slot 93, checks slot 104, and
calls `VClient017` slot 14 or 15. Each call site is exactly `+0xE0` after its
August offset in section 6.3.

#### Relocated functions and fields

Every other function the chapters cite by address was located in the new build
by searching for its masked August bytes. Each has exactly one match, and each
match agrees with the August function on at least 96% of its masked first 256
bytes:

| Function | August | 2026-09-20 |
| --- | ---: | ---: |
| `CClientState` constructor | `engine.dll+0xA0DF0` | `engine.dll+0xA0E70` |
| view/projection matrix builder (`GetMatricesForView` callee) | `engine.dll+0xD5580` | `engine.dll+0xD5600` |
| world-to-pixels builder | `engine.dll+0xD5910` | `engine.dll+0xD5990` |
| `CInput::CreateMove` real command path | `client.dll+0x152290` | `client.dll+0x151430` |
| extra-mouse-sample command path | `client.dll+0x1527B0` | `client.dll+0x151950` |
| prediction seed publish helper | `client.dll+0x524C0` | `client.dll+0x52270` |
| RunCommand-style caller | `client.dll+0x187440` | `client.dll+0x186530` |
| weapon fire/FX wrapper | `client.dll+0x236AB0` | `client.dll+0x235CA0` |
| weapon frame path (slot-384 caller) | `client.dll+0x235FC0` | `client.dll+0x2351B0` |
| accuracy helper reading `weapon+0xC62` | `client.dll+0x4D700` | `client.dll+0x4D4B0` |
| `CCSWeaponInfo` script parse | `client.dll+0x1FDC30` | `client.dll+0x1FCE00` |
| `FireBullet` direction | `client.dll+0x1F9BB0` | `client.dll+0x1F8D80` |
| sin/cos pair helper | `client.dll+0x38F140` | `client.dll+0x38EFC0` |
| player view punch add | `client.dll+0x54450` | `client.dll+0x54200` |
| `CViewRender::SetUpView` | `client.dll+0x1BB5E0` | `client.dll+0x1BA7A0` |
| `FrameStageNotify` stage-5 handler | `client.dll+0xD6D70` | `client.dll+0xD6B90` |
| `C_BaseAnimating::SetupBones` | `client.dll+0x7B4A0` | `client.dll+0x7B250` |
| active weapon handle resolver | `client.dll+0x4A890` | `client.dll+0x4A640` |
| entity-list networkable wrapper | `client.dll+0xDF750` | `client.dll+0xDF5B0` |
| `CTEFireBullets::PostDataUpdate` | `client.dll+0x1F0500` | `client.dll+0x1EF6D0` |

The hardcoded client-only fields also still appear with the same
displacements: the `SetupBones` cache reads `[R14+0xB50]`/`[R14+0xB40]`
(`client.dll+0x7BA09`/`+0x7BA1F`), the `GetIDTarget` read `[RDI+0x1B20]`
(`client.dll+0x1EB626`), and the weapon-info index `weapon+0xC62`.

#### Verdict

For the 2026-09-20 build, every signature, vtable slot, interface chain,
documented function, and hardcoded field offset that the x64 DLL uses has been
located and matches the August evidence. The only change the DLL needed was
the `ClientState` pattern. This is static evidence. It does not replace a
permitted local smoke test, which remains the gate before a feature's default
changes. Unless a table says otherwise, the module-relative addresses elsewhere
in chapters 004 through 006 describe the August build.

## Checklist for the next game update

1. Confirm the PE machine type and the actual loaded module paths.
2. Record the SHA-256 of every changed module next to the notes you take.
3. Re-import the changed x64 modules into Ghidra.
4. Re-run each byte pattern and require a unique match. Check patterns that
   keep exact call or jump displacements first; the old `ClientState`
   initializer pattern was the first to break. When re-deriving one, prefer a
   use site whose fixed bytes are opcodes and meaningful constants (section
   5.1).
5. Re-check the RIP-relative instruction length and operand offset.
6. Re-check interface names, object vtables, and method slots, including the
   `VGUI_Surface030` cursor slots. The fastest reliable method is the one in
   section 11.1: recover the last verified binaries from the Ghidra project and
   compare each vtable slot by slot.
7. Confirm the `ClientClass`/`RecvTable`/`RecvProp` assertions still describe
   the metadata ABI.
8. Keep unsafe client-only features disabled until their fields are proven.
   Re-check dormancy, the bone-cache pointer/count path, and client-only
   semantic fields such as `C_CSPlayer::GetIDTarget`, instead of searching
   only for old x86 member names.
9. Build a fresh architecture-specific directory and perform a permitted local
   smoke test. Check the config path, interface names, and logged match
   offsets first.
