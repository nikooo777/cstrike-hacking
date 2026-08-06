# Source engine reverse-engineering tutorial

Learn how Counter-Strike: Source fits together by building a small **x86/x64 internal experiment DLL**. This is a personal reverse-engineering and systems exercise, not a ready-to-use product.

> **Safety note:** Use this only for source study and permitted, offline/local testing. There is no VAC bypass, anti-cheat defeat, injector, or online-testing workflow here. Do not load it into protected multiplayer or someone else's process.

## Why this exists

I enjoy Source games and programming. C++ is not my favorite language, but it is the right tool for exploring client state, input, entities, and rendering at this level.

The practical goal is defensive understanding: learn what a client-side tool can read and change so I can better protect a large Counter-Strike: Source community. The code is intentionally small enough to follow while learning.

## Why CS:S?

- It is still played and interesting to study.
- It is old enough that updates are relatively infrequent, so signatures and offsets remain useful for longer.
- Its architecture is close enough to other Source-era games for research notes to transfer.
- There are enough public write-ups and dumps to cross-check findings.

The first guide I used was [Guided Hacking's beginner guide](https://guidedhacking.com/threads/ghb1-start-here-beginner-guide-to-game-hacking.5911/).

## What works today

This is a teaching codebase tied to one Counter-Strike: Source client build. It builds architecture-specific DLLs with CMake and MSVC and contains these experiments:

| Experiment | Hook / input | Implementation |
|------------|--------------|----------------|
| Bunny hop | `CreateMove` / hold **Space** | `features/bhop.cpp` writes the client's force-jump command. |
| Aimbot | `CreateMove` / hold **LMB** | `features/aimbot.cpp` chooses the closest valid enemy and aims at bone 14; x64 target validation uses `IClientNetworkable::IsDormant`. |
| Triggerbot | `CreateMove` / hold **Shift** | `features/triggerbot.cpp` uses the client-only crosshair target (`m_iIDEntIndex`); the x64 field is `player + 0x1B20`, but the x64 profile keeps the feature off until its complete input/target path is validated. |
| Command no-recoil | `CreateMove` | `features/norecoil.cpp` compensates view angles using the local punch angle. |
| Visual no-recoil | `FrameStageNotify` | Temporarily hides the punch angle during `FRAME_RENDER_START`, then restores it. |
| ImGui menu | `EndScene` + window procedure | `features/menu.cpp` exposes runtime toggles and is opened with **Insert**. |
| Debug dump | `CreateMove` / **F1** | `features/debug_info.cpp` prints module bases, offsets, client state, local dormancy, target counts, bone-cache state, and local position. |

The x86 profile starts all gameplay experiments enabled; the x64 profile now enables the aimbot after validating `IClientNetworkable::IsDormant`, while triggerbot remains disabled. The x64 `C_BaseAnimating::SetupBones` cache fields are reached through the renderable subobject: Ghidra shows `[this + 0xB40]`/`[this + 0xB50]`, which translate to a matrix pointer at `entity + 0xB48` and a count at `entity + 0xB58` from the entity-list pointer. The corrected path has been runtime-validated on the current x64 build (`count=50`, readable and usable); the F1 dump continues to report the raw values and readability flags so future updates can be checked. The menu itself starts closed. MinHook installs `CreateMove`, `FrameStageNotify`, and D3D9 `EndScene`. The window procedure is replaced separately so **Insert** works even when `CreateMove` is not running.

### In-game controls

| Key | Action |
|-----|--------|
| **INSERT** | Toggle the ImGui menu. |
| **F1** | Print the debug dump to the allocated console. |
| **END** | Disable hooks, restore the window procedure, and unload the DLL. |
| **Space** | Hold for bunny hop when enabled. |
| **Shift** | Hold for triggerbot when enabled. |
| **LMB** | Hold for the aimbot when enabled. |

The menu is initialized on the first successful D3D9 `EndScene` call. If it is closed, the hook skips ImGui's `NewFrame` and render work; **Insert** is still handled by the window procedure.

## Runtime flow

The DLL keeps the entry point small and does the work on a worker thread:

~~~text
DllMain (process attach)
  -> hooks::MainThread
       -> load signatures.ini beside the DLL and apply runtime settings
       -> resolve VClient017, ClientMode, ClientState, RecvTables, and a D3D9 vtable
       -> install MinHook hooks
       -> run feature code from the appropriate callback

CreateMove          -> bhop, triggerbot, aimbot, command no-recoil, F1 debug
FrameStageNotify    -> temporary visual no-recoil adjustment
EndScene            -> ImGui frame/render when the menu is open
Window procedure     -> Insert toggle and ImGui input
END                 -> disable hooks, shut down ImGui, unload the DLL
~~~

## Project layout

```text
Nikooo777/
  dllmain.cpp              # DllMain only — starts the main thread
  core/                    # constants, padding macros, module bases
  config/                  # runtime INI loader
  memory/                  # pattern scanner (ScanModCombo, module size, …)
  math/                    # Vector3 (POD so it works in overlay unions)
  netvars/                 # runtime ClientClass/RecvTable/RecvProp resolver
  sdk/                     # Source-like types only (no feature logic)
    entity/                # CLocal, CBasePlayer, CCSPlayer
    user_cmd.h, client_*.h, engine_client.h, client_entity_list.h, create_interface.*
  game/                    # live game access
    entity_list.*          # local player + IClientEntityList access
    interfaces.*           # interfaces, EngineClient / ClientMode / ClientState resolve
    player.*               # IsAlive, IsEnemy, IsValidTarget, EyePosition
  hooks/                   # MinHook lifecycle + individual hooks + dummy D3D device
  features/                # gameplay logic + menu + config flags
imgui/                     # Dear ImGui + DX9 / Win32 backends
minhook/                   # vendored source, headers, license, and legacy x86 libs
config/                    # architecture-specific signatures, settings, provenance
```

### Layer rules (keep the tutorial readable)

1. **`sdk/`** — memory layouts and interface stubs. No hooks, no features.
2. **`game/`** — how we *find* and *read* live objects (signatures, entity list).
3. **`features/`** — what we *do* with that data. Prefer `game::` helpers over copy-pasted field checks.
4. **`hooks/`** — only place that installs MinHook / D3D and calls into features.
5. **`dllmain.cpp`** — attach / detach only.

Networked entity members use runtime `RecvTable` accessors (`DEFINE_NETVAR`) so their
displacements come from the loaded client metadata. Client-only fields are kept
explicit only after they are verified for the selected architecture; the x64
crosshair target is the `C_CSPlayer::GetIDTarget` field at `player + 0x1B20`.
View angles are read through the named `VEngineClient` interface at vtable slot
19 instead of a copied x64 `ClientState` overlay. Entity lookup and input
buttons use the named interface/command sources documented in `aidocs/003`.

### Where to add something new

| Goal | Place |
|------|--------|
| New cheat feature | `features/foo.*` → call from `hooks/create_move.cpp` (logic) or `hooks/end_scene.cpp` (draw) → add `.cpp` to `CMakeLists.txt` → optional toggle in `features/config.h` + menu |
| Networked player / entity field | Resolve and add its `RecvTable` path in `sdk/entity/` with `DEFINE_NETVAR`; document the discovery in `aidocs/002_netvars-and-entity-offsets.md`. |
| Client-only entity field | `sdk/entity/` with `DEFINE_MEMBER`, after verifying that it is not in a receive table. |
| Global address or input slot | Prefer a named interface or `CUserCmd`; document a true signature in `aidocs/003_global-addresses-and-inputs.md` when no semantic source exists. |
| New interface / signature | Add the pattern, operand rule, and provenance to the matching architecture profile (`config/signatures.ini` for x86 or `config/signatures-x64.ini` for x64); keep resolution logic in `game/interfaces.cpp` and explain discovery in `aidocs/`. |
| Shared target / eye helpers | `game/player.*` |

## Building

### Requirements

- **Windows**, with an MSVC profile matching the target game process: x86 for the legacy client or x64 for the updated client
- **CMake** ≥ 3.19
- **MSVC** with a toolset matching the selected architecture (VS 2019 Build Tools work)
- **[DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812)** — `d3d9` only (ImGui’s DX9 backend does not need D3DX). Default path in CMake; override with `-DDXSDK_DIR=...`
- **C++17**

Dear ImGui and the MinHook source/headers are included in the repository, so no package manager is required. The CMake file links only `d3d9`; ImGui’s DX9 backend does not require D3DX.

MinHook is built from the vendored source for the selected pointer size. The old
prebuilt x86 libraries remain in `minhook/lib/` as historical reference, but
the CMake target no longer depends on them. VS 2019 Build Tools are the baseline
used here; a compatible newer MSVC toolset should also work.

For CLion, select an **MSVC x86 or x64** CMake profile matching the game. Its
bundled MinGW toolchain is not the baseline for this Windows DLL. With Visual
Studio, choose the platform that matches the loaded game.

### MSVC x86 command line

Open an **x86 Native Tools Command Prompt for Visual Studio**. If you start from a regular cmd.exe, initialize the x86 environment first:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
```

Configure and build a Debug DLL:

```bat
cmake -S . -B build-msvc-x86 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msvc-x86 --target nikooo777
```

If the DirectX SDK is installed elsewhere, point DXSDK_DIR at its root directory—the directory containing Include and Lib:

```bat
cmake -S . -B build-msvc-x86 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDXSDK_DIR="D:/SDKs/DirectX SDK (June 2010)"
```

Debug output: `build-msvc-x86/nikooo777.dll`.

The build also copies `config/signatures.ini` to `build-msvc-x86/signatures.ini`, beside the DLL. Edit the checked-in file, then rebuild before testing a new binary.


For a single-config NMake Release build:

```bat
cmake -S . -B build-msvc-x86-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-msvc-x86-release --target nikooo777
```

MinHook is compiled in the selected configuration. For a multi-configuration Visual Studio generator, use `cmake --build <build-dir> --config Debug` or `--config Release`. Use a fresh build directory when switching architectures.

### MSVC x64 command line

The updated game uses the x64 module set. Open an x64 Native Tools Command Prompt
or initialize it from a regular prompt:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -S . -B build-msvc-x64 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msvc-x64 --target nikooo777
```

This selects `Lib/x64/d3d9.lib`, builds the x64 MinHook sources, and copies
`config/signatures-x64.ini` to `build-msvc-x64/signatures.ini`. Do not reuse an
x86 build directory when changing pointer size.

## Permitted offline smoke test

This repository does not provide a standalone executable, injector, or anti-cheat workaround. For a local test environment you control:

1. Build the DLL for the same architecture as the target game process.
2. Start a permitted offline or local Counter-Strike: Source session.
3. Load the DLL using an injector you already trust and are authorized to use.
4. Check the console for `BaseClient`, `Netvars initialized`, `ClientEntityList`, `ClientMode`, `FrameStageNotify`, `CreateMove`, and `EndScene` addresses.
5. Press **F1** to print the module/interface/netvar dump, then **Insert** to open the menu.
6. Press **End** to restore hooks and unload cleanly.
7. Confirm the console shows the loaded config path and the signature provenance/match offsets before treating a resolution as valid.

The addresses, offsets, and signatures are build-specific. A successful DLL build does not mean that it is safe to load into a different game binary.

## Offsets, signatures, and vtables

These are the files to revisit when the client build changes:

| File | What it contains |
|------|------------------|
| `netvars/netvars.*` | Runtime `ClientClass`/`RecvTable` traversal and entity-relative offsets. |
| `sdk/client_entity_list.h` / `game/entity_list.cpp` | Named `VClientEntityList003` interface and player-slot translation. |
| `sdk/entity/*.h` | Netvar-backed entity accessors plus explicitly client-only fields and the remaining padded layout. |
| `config/signatures.ini` / `config/signatures-x64.ini` | Architecture-specific patterns, named interfaces, operand decoders, pointer indirections, validation settings, feature defaults, and discovery links. |
| `game/interfaces.cpp` | `CreateInterface` lookup plus configured entity-list, EngineClient, ClientState, and ClientMode resolution logic. |
| `hooks/hooks.cpp` | Vtable slots for `CreateMove`, `FrameStageNotify`, and `EndScene`. |
| `core/constants.h` | Entity stride, player limits, team values, and movement flags. |

The debug dump reports module bases, runtime interface/client-only offsets, resolved netvar offsets, the resolved ClientState address, view angles from `VEngineClient`, the local bone-cache pointer/count, and the local player position when one is available. If a signature or required netvar is not found, or a read produces null/garbage data, treat the binary and the offsets as mismatched and re-dump them rather than guessing.

## Tutorial notes

- [001 - Signature scanning, offsets, and pointer derivation](aidocs/001_signature-scanning-and-offsets.md)
- [002 - Netvars and entity offsets](aidocs/002_netvars-and-entity-offsets.md)
- [003 - Global addresses, interfaces, and input commands](aidocs/003_global-addresses-and-inputs.md)
- [004 - x64 migration and ABI](aidocs/004_x64-migration-and-abi.md)

The architecture-selected signature profile is the source of truth for the two
runtime signatures and the named client/engine interfaces. It intentionally
records how each pattern or interface was found, not just the bytes: update the
provenance fields whenever a new build is reverse-engineered.

## Troubleshooting

| Symptom | Likely cause / next check |
|---------|---------------------------|
| CMake selects the wrong architecture | Check `CMAKE_SIZEOF_VOID_P`, initialize the matching MSVC environment, and configure a fresh build directory. |
| `d3d9.h` or `d3d9.lib` is missing | Set `DXSDK_DIR` to the DirectX SDK root and verify Include/d3d9.h plus `Lib/x86/d3d9.lib` or `Lib/x64/d3d9.lib` exist. |
| A MinHook library cannot be opened | MinHook is built from source; reconfigure after adding the vendored `minhook/src` files and inspect the selected C/C++ toolchain. |
| `signatures.ini` cannot be loaded | Build from the repository so CMake copies the architecture-selected profile beside the DLL; do not launch with a stale or missing adjacent config. |
| `ClientState signature not found` or `ClientMode signature not found` | The byte pattern is for another client build. Confirm the executable/module version and update the pattern. |
| `BaseClient is null` | `VClient017` was not exposed by the loaded client module, or the DLL was loaded at the wrong time/process. |
| `ClientEntityList interface not found` | `VClientEntityList003` is unavailable or the configured client module is wrong. Re-check the interface name and the selected ABI. |
| `Failed to initialize netvars` | `GetAllClasses`, a required RecvTable, or a required property does not match this client build. Stop and re-check the table path and pointer-width assertions. |
| D3D9 capture fails or the menu never appears | The code needs a visible, suitably sized game window and a D3D9 device. Wait until the game window is initialized and verify that the target is using D3D9. |
| A feature crashes or reads implausible values | Stop testing: an entity/global offset is stale or the target is not the expected architecture/build. Disable the feature and return to Ghidra. |

## Known limitations

- Signatures, interface versions, vtable assumptions, and remaining client-only fields are tied to the client build this project was developed against.
- Netvars remove several hardcoded entity displacements, but the `ClientClass`/`RecvTable` ABI, table names, property names, and type assumptions are still build-family dependencies.
- Memory access is direct and lightly validated; this is experimental code, not a hardened runtime.
- The aimbot is intentionally basic: closest valid target, bone 14, and an immediate angle change. It does not implement visibility checks, smoothing, weapon handling, or movement correction.
- The triggerbot is deliberately narrow: it uses the client-only crosshair target ID and requires the local player to be on the ground.
- Rendering support is D3D9-specific and depends on finding the game's visible top-level window.
- There is no automated test suite; validation is a successful architecture-matched build followed by a permitted local smoke test.
- Nothing here is intended to bypass VAC, FaceIT, or any other anti-cheat system.

## Videos / notes while reversing

| Topic | Link |
|-------|------|
| Finding ClientState | [Odysee](https://odysee.com/@Swiss-Experiments:a/finding-clientstate-with-ida-on-counter:7) · [YouTube](https://www.youtube.com/watch?v=J6vO-ANi4Q8) |
| Finding view angles | [Odysee](https://odysee.com/@Swiss-Experiments:a/finding-viewangles-with-ida-for-counter:e) · [YouTube](https://www.youtube.com/watch?v=mS8ZQ5N7Dvk) |
| Finding bone matrix | [Odysee](https://odysee.com/@Swiss-Experiments:a/how-to-locate-bonematrix:5) · [YouTube](https://www.youtube.com/watch?v=elKUMiqitxY) |

Remaining client-only offsets in the entity headers are for the client build this project was developed against. The x64 bone-cache offsets documented in `aidocs/004` came from the current `SetupBones` implementation, but they still need to be re-dumped if the binary changes.

The signature records point back to this section and the numbered tutorial so the pattern bytes, operand offsets, and pointer-chain assumptions can be re-derived instead of copied blindly.

## Status / honesty

- Code quality is tutorial and experimental, not production.
- There is no support for protected multiplayer or anti-cheat bypass.
- If you need a maintained end-user package, this repository is intentionally not that.

## End goal

Understand the client well enough to defend a large CS:S community: know what cheats can see and do, and how to reason about them when they show up on the server.
