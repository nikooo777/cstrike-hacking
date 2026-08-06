# 004 — Porting the experiment from x86 to x64

This chapter records the first x64 pass against the updated Counter-Strike:
Source client. It is deliberately tied to the current binaries on disk, not a
claim that one set of offsets will survive every update.

The useful mental model is:

```text
x86 build                         x64 build
---------                         ---------
32-bit pointers                   64-bit pointers
absolute imm32 globals            RIP-relative rel32 operands
__thiscall + __fastcall thunk     one Microsoft x64 calling convention
4-byte RecvTable pointers          8-byte RecvTable pointers
prebuilt x86 MinHook              source-built MinHook for the target ABI
```

The target modules are now split between the game directory and architecture
subdirectories. The x64 set used for this pass is:

```text
cstrike/bin/x64/client.dll
cstrike/bin/x64/server.dll
bin/x64/engine.dll
bin/x64/vgui2.dll
bin/x64/vguimatsurface.dll
```

The process still exposes them by the familiar names (`client.dll`,
`engine.dll`, and so on), so the runtime module names do not change. The DLL
must, however, be built x64 and loaded into the x64 process.

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

Create a separate Ghidra project or at least separate program imports. Do not
reuse the old x86 program's addresses or data types. For this pass the useful
programs were:

| Program | Image base | Ghidra language |
|---|---:|---|
| `client.dll` | `0x180000000` | `x86:LE:64:default` |
| `engine.dll` | `0x180000000` | `x86:LE:64:default` |

Keep module-relative addresses in notes. ASLR changes the loaded base, while a
match such as `client.dll+0x1F1443` is directly comparable with the runtime
log.

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

It is not correct to read the four bytes as a complete pointer. The new
`mem::DecodeRipRelative32` helper reads a signed 32-bit displacement and adds
it to the end of the instruction. The configuration records the operand offset,
the instruction's offset inside the signature, and the instruction length
because the latter is part of the address calculation. This distinction
matters when a signature starts with setup bytes before the RIP-relative
instruction.

The resolver accepts both operand descriptions:

```text
abs32       x86 absolute imm32 used by the original profile
rip_rel32   x64 signed displacement relative to the next instruction
```

This keeps the scanner architecture-neutral while keeping the binary-specific
evidence in the configuration file.

## 4. Re-find ClientMode in the x64 client

In Ghidra, the x64 ClientMode evidence is:

1. RTTI identifies `ClientModeShared`.
2. Its vtable is at `client.dll+0x42DF88` in this sample.
3. `CreateMove` remains vtable slot 21, matching
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

The config representation is on one line in
`config/signatures-x64.ini`:

```ini
operand=rip_rel32
operand_offset=3
instruction_offset=0
instruction_length=7
indirections=0
```

The first RIP-relative operand points directly at the object, so there is no
second pointer read. That is different from the x86 profile, where the
absolute operand points at a global slot containing `ClientMode*` and
`indirections=1` is correct.

## 5. Re-find ClientState in the x64 engine

The x64 engine contains the `CClientState` RTTI and a constructor at
`engine.dll+0xA0DF0`. Its static initialization path at `engine.dll+0x47E0`
passes the live object at `engine.dll+0x535810` to that constructor.

A short wildcarded initializer pattern is ambiguous: many C++ static
initializers have the same stack adjustment, LEA, call, cleanup, and jump
shape. Ghidra returned many matches for the fully wildcarded form. Keeping the
verified call and tail-jump bytes exact makes the current pattern unique:

```text
48 83 EC 28 48 8D 0D ?? ?? ?? ??
E8 00 C6 09 00
48 8D 0D ?? ?? ?? ?? 48 83 C4 28
E9 6C F7 30 00
```

The exact match check returned only `0x1800047E0`. Its first LEA begins four
bytes into the signature, so it uses `operand_offset=7`,
`instruction_offset=4`, and `instruction_length=7`; it resolves directly to
the ClientState object (`indirections=0`). The exact branch bytes are a deliberate
trade-off for this particular build: if a future update changes those local
branches, repeat the Ghidra uniqueness check and update the profile.

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

The x64 `VClient017` object is at `client.dll+0x5AE6D8`. Its vtable confirms:

```text
GetAllClasses       slot 8
IN_ActivateMouse    slot 14
IN_DeactivateMouse  slot 15
FrameStageNotify    slot 35
```

The `VClientEntityList003` object is at `client.dll+0x6498C8`. Its vtable
still exposes `GetClientEntity` at slot 3. The pointer width changes, but the
method order is checked against the Source SDK header rather than inferred
from the old object address.

The x64 ClientMode vtable keeps `CreateMove` at slot 21. D3D9's
`IDirect3DDevice9::EndScene` remains slot 42. VGUI method slots should still
be verified against the loaded x64 VGUI interfaces before changing cursor or
input behavior; an interface name alone does not prove a vtable layout.

The engine view-angle path is another good example of preferring a semantic
interface over a guessed structure overlay. The current x64 engine registers
`VEngineClient014`. The Source SDK declaration places `GetViewAngles(QAngle&)`
at vtable slot 19 (with `SetViewAngles` at slot 20). In Ghidra, the x64
interface object is returned by the factory at `engine.dll+0x71440`, whose LEA
resolves `engine.dll+0x4605A0`; its vtable is at `engine.dll+0x366070`.
Slot 19 points to `engine.dll+0x70630`, and that function copies three floats
from `engine.dll+0x53E4E4`, `+0x53E4E8`, and `+0x53E4EC` into the caller's
`QAngle` buffer. The implementation calls slot 19 through the configured
`VEngineClient014` interface, so no x64 `ClientState` view-angle displacement
is assumed.

## 7. Update the metadata ABI

The netvar walker reads game-owned `ClientClass`, `RecvTable`, and `RecvProp`
objects. Pointer fields cannot be represented by `DWORD` in x64. The current
declarations use pointer types and compile-time assertions document the
expected offsets:

| Field | x86 | x64 |
|---|---:|---:|
| `ClientClass::recvTable` | `0x0C` | `0x18` |
| `RecvProp::offset` | `0x2C` | `0x48` |
| `RecvTable::name` | `0x0C` | `0x18` |

The values are not entity offsets. They are offsets inside the metadata
structures themselves. If one assertion fails, stop and re-check the ABI
before trying to compensate with a different netvar path.

Likewise, an entity field that stores a pointer must use `std::uintptr_t` (or
another pointer type). The bone-matrix member is one such field. A 32-bit
`DWORD` would truncate the x64 address even if the entity-relative field
displacement happened to remain unchanged.

### 7.1 Trace the bone cache through `SetupBones`

The old x86 overlay used `entity + 0x578` as the cached bone-matrix pointer.
That value cannot be carried into x64 by habit. In the x64 client, search the
`C_BaseAnimating::SetupBones` string and follow its reference. The current
Ghidra program identifies the function at `client.dll+0x7B4A0`.

The cached-copy path is the useful evidence:

```asm
MOVSXD R8, dword ptr [R14 + 0xB50] ; cached bone count
MOV    RDX, qword ptr [R14 + 0xB40] ; m_CachedBoneData.Base()
...
CALL   FUN_1803D6520              ; copy count * 0x30 bytes
```

The important ABI detail is what `R14` means. `SetupBones` is dispatched
through `IClientRenderable`, so its `this` pointer is the renderable
subobject, eight bytes after the `IClientEntity` pointer returned by
`GetClientEntity`. The nearby `R14 - 8` expressions recover the main entity
object and its vtable. Therefore the raw Ghidra displacements above must be
translated by `+0x8` when the code starts from the entity-list pointer:

```text
SetupBones this + 0xB40 -> entity + 0xB48: bone matrix pointer
SetupBones this + 0xB50 -> entity + 0xB58: bone count
matrix stride:       0x30 bytes
bone 14 position:    matrix + 0x0C, +0x1C, +0x2C
```

The implementation now reads the translated `entity + 0xB48` and
`entity + 0xB58` fields and the F1 dump prints the raw pointer, count, and
readability flags even when the cache is not populated. A zero pointer/count
after the corrected read would indicate that the cache has not been filled at
the point of the `CreateMove` diagnostic; a nonzero, plausible pair confirms
the layout. This still does not prove that the x64 dormant state is resolved;
section 7.2 adds that semantic path. The x64 aimbot can use it, while
triggerbot remains disabled until its complete path is validated.

The current x64 runtime pass confirmed the translation: the F1 dump reported
`matrixReadable=yes`, `countReadable=yes`, `usable=yes`, and a bone count of
`50` for the local player. The absolute entity and matrix addresses are
intentionally omitted because ASLR changes them between launches. This proves
the cache pointer/count path for this build; the separate dormancy diagnostic
reports whether the networkable interface can be resolved in the same run.

### 7.2 Resolve dormancy through the networkable interface

The bone cache can be valid while the aimbot still selects no targets. The
first x64 profile deliberately set `aimbot=false`, and its temporary
`m_bDormant()` implementation returned `true` for every entity. That made
`IsValidTarget()` reject all players by design.

For this build, the safer semantic path is the client networkable interface:
the x64 entity-list wrapper at `client.dll+0xDF750` reaches the entity's
`IClientUnknown::GetClientNetworkable` entry at vtable slot 4, and the Source
SDK declaration places `IClientNetworkable::IsDormant` at slot 8. The runtime
now validates both vtable function addresses, calls those two methods, and
fails closed if either interface cannot be resolved. The x64 aimbot profile is
enabled again; triggerbot remains disabled until its whole path is validated.
The F1 dump reports the local resolver state plus the number of valid targets
and readable bone-14 matrices, which separates target filtering from angle
application when testing the feature.

## 8. Fix the hook ABI

On x86, the detours use an x86-compatible shape:

```cpp
bool __fastcall hook(void *thisPtr, void *edx,
                     float sampleTime, CUserCmd *cmd);
```

The extra `edx` parameter is the familiar way to bridge a `__thiscall` target
with a `__fastcall` detour. MSVC x64 uses one calling convention and does not
pass that placeholder. The x64 declarations therefore use:

```cpp
bool hook(void *thisPtr, float sampleTime, CUserCmd *cmd);
```

The project keeps the x86 declarations under `_M_IX86` and the x64
declarations under the unified branch. `__thiscall`, `__fastcall`, and
`__stdcall` should not be treated as portable annotations when reviewing an
x64 hook signature; compare the actual parameter list and register ABI.

## 9. Build the correct profile

The CMake file now derives the architecture from pointer size:

```text
CMAKE_SIZEOF_VOID_P == 4  -> x86 profile + config/signatures.ini
CMAKE_SIZEOF_VOID_P == 8  -> x64 profile + config/signatures-x64.ini
```

MinHook is built from the [official MinHook source repository](https://github.com/TsudaKageyu/minhook)
vendored under the project for the selected architecture, rather than linking
only the old prebuilt x86 libraries. The relevant source and license are in
`minhook/src`, `minhook/include`, and `minhook/LICENSE.txt`.

From an x64 Native Tools command prompt:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -S . -B build-msvc-x64 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msvc-x64 --target nikooo777
```

The DirectX SDK path can still be overridden with `-DDXSDK_DIR=...`; CMake
selects `Lib/x64/d3d9.lib` for this profile. It copies the selected profile to
`build-msvc-x64/signatures.ini`, next to the DLL.

## 10. What is deliberately not claimed yet

The core x64 pass now has architecture-correct build, signature decoding,
interface discovery, netvar metadata traversal, client-only target discovery,
and hook declarations. The remaining unenabled client-only experiment is
deliberately conservative:

- The x64 client does not expose the crosshair target as a receive-table
  property. Semantic tracing from `CTargetID` to `C_CSPlayer::GetIDTarget`
  identifies the current target field at `player + 0x1B20`; Ghidra shows the
  accessor reading `[RDI + 0x1B20]` at `client.dll+0x1EC456` and the update
  path writing the same displacement. The x64 profile can therefore enable
  Triggerbot without pretending that a netvar named `m_iCrosshairID` exists.
  The profile still leaves Triggerbot disabled while its complete input and
  target path is validated.
- The x64 bone cache is mapped from `C_BaseAnimating::SetupBones`: Ghidra's
  renderable-subobject accesses are `[this + 0xB40]` and `[this + 0xB50]`,
  which become a matrix pointer at `entity + 0xB48` and a count at
  `entity + 0xB58` from the entity-list pointer. The F1 dump now reports raw
  values and readability so cache timing can be distinguished from a bad
  displacement. The aimbot reads bone 14 through a bounds-checked helper,
  and the x64 profile enables it after the separate dormancy interface path
  was mapped.
- The x86 dormant member is not reused on x64; x64 target validation now calls
  `IClientNetworkable::IsDormant` through the verified interface slots and
  fails closed if the call cannot be resolved.
- The x86 ClientState view-angle overlay is not reused on x64. The debug dump
  now obtains angles through `VEngineClient014::GetViewAngles` slot 19.

These are useful stopping points. A successful DLL build and successful
interface resolution do not validate every unverified client-only field. The
next pass can validate the triggerbot's crosshair/input path and enable it only
after that runtime check.

## Checklist for the next game update

1. Confirm the PE machine type and the actual loaded module paths.
2. Re-import the changed x64 module into Ghidra.
3. Re-run each byte pattern and require a unique match.
4. Re-check the RIP-relative instruction length and operand offset.
5. Re-check interface names, object vtables, and method slots.
6. Confirm the `ClientClass`/`RecvTable`/`RecvProp` assertions still describe
   the metadata ABI.
7. Keep unsafe client-only features disabled until their fields are proven;
   re-check both dormant validation and the bone-cache pointer/count path.
8. Re-check client-only semantic fields such as `C_CSPlayer::GetIDTarget`
   instead of searching only for old x86 member names.
9. Build a fresh architecture-specific directory and perform a permitted local
   smoke test, checking the config path, interface names, and logged match
   offsets first.
