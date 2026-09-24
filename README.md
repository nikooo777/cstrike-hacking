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
| Aimbot | `CreateMove` / hold **LMB** | `features/aimbot.cpp` checks bone 14 and `IEngineTrace` visibility, then chooses the closest visible enemy; the candidate set is rebuilt every tick so dead or unreadable targets fall through to the next candidate. |
| Triggerbot | `CreateMove` / hold **Shift** | `features/triggerbot.cpp` uses the client-only crosshair target (`m_iCrosshairID`); the x64 field is `player + 0x1B20`, but the x64 profile keeps the feature off until its complete input/target path is validated. |
| Command no-recoil | `CreateMove` | `features/perfect_nospread.cpp` composes the configured punch policy with aim and spread in fire space; `features/norecoil.cpp` supplies checked punch state. |
| Perfect no-spread | `CreateMove` | Replays the verified x64 CS polar cone and applies inverse command-angle compensation; default off because the feature remains build-specific and exploratory. |
| Silent angles | `CreateMove` return value | Prevents compensated command angles from being copied into the render camera. |
| Visual no-recoil | `ClientMode::OverrideView` | Subtracts punch from the completed camera view without changing player state. |
| Bone ESP | `EndScene` | Experimental read-only skeleton lines using the verified studio hierarchy, cached bone matrices, and world-to-screen projection; disabled by default. |
| Menu wheel | `EndScene` + window procedure + `VGUI_Surface030::LockCursor` | A radial menu around the crosshair, opened with **Insert** (chapter 007). `ui/` draws the categories, toggles, and a readout that explains each item; `features/menu.cpp` connects it to the config flags and to the live hook, setup, shot, and Bone ESP status in `features/telemetry.*`. The cursor hook prevents the engine from re-locking the mouse while it is open. |
| Debug dump | `CreateMove` / **F1** | Prints resolved state plus command, recoil, spread, fire-time, bone-cache, hierarchy, and projection diagnostics. |

The x86 profile retains the legacy triggerbot experiment, while the x64 profile keeps triggerbot disabled and enables the aimbot after validating `IClientNetworkable::IsDormant`. Both profiles keep perfect no-spread and Bone ESP off by default. The current Bone ESP path is x64-first: Ghidra verifies `VEngineRenderView014::GetMatricesForView` at slot 50, `VModelInfoClient006::GetStudiomodel` at slot 28, and the entity renderable's `GetModel` at slot 9. It reads dynamic studio parent links and the validated x64 `C_BaseAnimating::SetupBones` cache through the renderable subobject (`entity + 0x8`), then projects matrix positions through the captured view. The x64 cache fields are `[this + 0xB40]`/`[this + 0xB50]` on the renderable, translating to a matrix pointer at `entity + 0xB48` and a count at `entity + 0xB58`; the current build reports `count=50`, readable and usable. The F1 dump reports hierarchy, matrix, and projected-line status so an update can be investigated without guessing. The menu itself starts closed. MinHook installs `CreateMove`, `ClientMode::OverrideView`, `VGUI_Surface030::LockCursor`, and D3D9 `EndScene` and `Reset` (plus `ResetEx` when a D3D9Ex device can be created, and the two fire-time capture detours on x64). **Insert** is polled in `EndScene`, so it works even when `CreateMove` is not running.

The matrix builder consumes the complete x64 `CViewSetup` layout, not just its `angles` field: the overlay is `0xC8` bytes and includes the near/far planes, aspect/off-center flags, and the override matrix through `+0x88`. This is asserted in `sdk/view_setup.h` and is required for valid world-to-screen output.

### In-game controls

| Key | Action |
|-----|--------|
| **INSERT** | Open or close the menu wheel. |
| **F1** | Print the debug dump to the allocated console. |
| **END** | Disable hooks, restore the window procedure, and unload the DLL. |
| **Space** | Hold for bunny hop when enabled. |
| **Shift** | Hold for triggerbot when enabled. |
| **LMB** | Hold for the aimbot when enabled. |

With the wheel open, click a category on the outer ring or scroll to change category, then click an item on the inner ring to toggle it. The center explains whatever the pointer is on, and the Status category shows whether each part of the DLL is working.

The menu and optional bone overlay are initialized on the first successful D3D9 `EndScene` call. Once ImGui is initialized, the hook runs a frame whenever drawing is enabled so Bone ESP can render while the menu is closed; the menu wheel itself is submitted only while open. **Insert** is polled at the start of each `EndScene`; the window procedure forwards input to ImGui while the menu is open. While the menu is open, the verified `VGUI_Surface030::LockCursor` hook substitutes `UnlockCursor` and an arrow cursor. The engine then observes the unlocked surface and deactivates first-person mouse recentering through its normal input path. This path has been runtime-validated on x64: the pointer moves freely as soon as the menu opens, without first opening the console or settings.

## Runtime flow

The DLL keeps the entry point small and does the work on a worker thread:

~~~text
DllMain (process attach)
  -> hooks::MainThread
       -> load signatures.ini beside the DLL and apply runtime settings
       -> resolve client, engine, trace, VGUI, RecvTable, and D3D9 objects
       -> install MinHook hooks
       -> run feature code from the appropriate callback

CreateMove          -> buttons, aim, command recoil/spread composition, F1 debug
OverrideView        -> read-only visual punch removal from the camera view
LockCursor          -> preserve an unlocked OS cursor while the menu is open
EndScene            -> Insert toggle, ImGui frame: menu wheel, optional Bone ESP beneath it
Reset / ResetEx     -> release ImGui's device objects before the device resets
Window procedure    -> ImGui input while the menu is open
END                 -> disable hooks, shut down ImGui, unload the DLL
~~~

## Project layout

```text
Nikooo777/
  dllmain.cpp              # DllMain only — starts the main thread
  core/                    # constants, padding macros, module bases, ARCH_X64()/ARCH_X86()
  config/                  # runtime INI loader
  memory/                  # pattern scanner, checked reads, checked vtable slots (mem::GetVirtual)
  math/                    # Vector3 and checked world-to-screen projection
  netvars/                 # runtime ClientClass/RecvTable/RecvProp resolver
  sdk/                     # Source-like types only (no feature logic)
    entity/                # CLocal, CBasePlayer, CCSPlayer
    client_offsets.h       # every build-specific client-only offset, with its aidocs section
    user_cmd.h, client_*.h, engine_*.h, vgui_surface.h, view_setup.h, create_interface.*
  game/                    # live game access
    entity_list.*          # local player + IClientEntityList access
    interfaces.*           # interfaces, view matrices, EngineClient / EngineTrace / ClientMode / ClientState resolve
    model.*                # checked studio-model hierarchy lookup
    render_state.*         # final OverrideView snapshot for render features
    player.*               # IsAlive, IsEnemy, IsValidTarget, EyePosition
  hooks/                   # MinHook lifecycle + individual hooks + dummy D3D device
  features/                # gameplay logic + menu + Bone ESP + F1 dump + fire-time capture + config flags + telemetry
  ui/                      # menu wheel geometry, theme, fonts, content, and drawing (ImGui only)
imgui/                     # Dear ImGui + DX9 / Win32 backends
minhook/                   # vendored source, headers, license, and legacy x86 libs
config/                    # architecture-specific signatures, settings, provenance
tools/                     # build-comparison scripts, the Ghidra byte-export script, and the offline wheel preview
```

### Layer rules (keep the tutorial readable)

1. **`sdk/`** — memory layouts and interface stubs. No hooks, no features.
2. **`game/`** — how we *find* and *read* live objects (signatures, entity list).
3. **`features/`** — what we *do* with that data. Prefer `game::` helpers over copy-pasted field checks.
4. **`ui/`** — the menu wheel's drawing and fixed content. ImGui only: no game, hook, or config access, so it can be tested and rendered offline.
5. **`hooks/`** — only place that installs MinHook / D3D and calls into features.
6. **`dllmain.cpp`** — attach / detach only.

Networked entity members use runtime `RecvTable` accessors (`DEFINE_NETVAR`) so their
displacements come from the loaded client metadata. Client-only fields live in
`sdk/client_offsets.h`, and only after they are verified for the selected
architecture; the x64 crosshair target is the `C_CSPlayer::GetIDTarget` field at
`player + 0x1B20`.
View angles are read through the named `VEngineClient` interface at vtable slot
19 instead of a copied x64 `ClientState` overlay. Entity lookup and input
buttons use the named interface/command sources documented in `aidocs/003`.

### Where to add something new

| Goal | Place |
|------|--------|
| New cheat feature | `features/foo.*` → call from `hooks/create_move.cpp` (logic) or `hooks/end_scene.cpp` (draw) → add `.cpp` to `CMakeLists.txt` → optional toggle in `features/config.h` + a menu wheel item (`aidocs/007`, section 10) |
| Networked player / entity field | Resolve and add its `RecvTable` path in `sdk/entity/` with `DEFINE_NETVAR`; document the discovery in `aidocs/002_netvars-and-entity-offsets.md`. |
| Client-only entity field | Add the offset to `sdk/client_offsets.h` with its aidocs section, after verifying that it is not in a receive table; expose it from `sdk/entity/` with `DEFINE_MEMBER` or read it in `game/`. |
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

### Offline deterministic tests

Three small architecture-specific test executables cover the pure math.
`weapon_math_tests` checks the seed derivation, cone replay, command layout, and
inverse-cone math; `projection_tests` checks the world-to-screen convention used
by Bone ESP; `wheel_tests` checks the menu wheel's sectors and hit testing. They
do not load the game, resolve signatures, or call game-owned interfaces:

```bat
cmake --build build-msvc-x64 --target weapon_math_tests projection_tests wheel_tests
ctest --test-dir build-msvc-x64 --output-on-failure
```

Run the same two commands with `build-msvc-x86` after configuring the x86
profile. With a multi-configuration Visual Studio generator, append
`--config Release` to the build and `-C Release` to `ctest`. These tests catch
deterministic math and ABI regressions; they do not
replace a permitted in-game smoke test for signatures, vtables, timing, or
object lifetimes.

The `wheel_preview` target is not a test. It renders the real menu wheel code
with sample data to PNG files, in software, so a design change can be reviewed
without the game. Chapter 007, section 8 shows how to build it on Linux too.

## Permitted offline smoke test

This repository does not provide a standalone executable, injector, or anti-cheat workaround. For a local test environment you control:

1. Build the DLL for the same architecture as the target game process.
2. Start a permitted offline or local Counter-Strike: Source session.
3. Load the DLL using an injector you already trust and are authorized to use.
4. Check the console for `BaseClient`, `Netvars initialized`, `ClientEntityList`, `ClientMode`, `OverrideView`, `CreateMove`, `LockCursor`, `EndScene`, and `Reset` addresses.
5. Press **F1** to print the module/interface/netvar dump, then **Insert** and confirm the pointer moves freely without another UI being open. In the wheel, **Status** should show **Hooks** and **Setup** as working.
6. Change the resolution once, or alt-tab out of fullscreen and back, and confirm the game recovers and the wheel still draws.
7. Press **End** to restore hooks and unload cleanly.
8. Confirm the console shows the loaded config path and the signature provenance/match offsets before treating a resolution as valid.

The addresses, offsets, and signatures are build-specific. A successful DLL build does not mean that it is safe to load into a different game binary.

## Offsets, signatures, and vtables

These are the files to revisit when the client build changes:

| File | What it contains |
|------|------------------|
| `netvars/netvars.*` | Runtime `ClientClass`/`RecvTable` traversal and entity-relative offsets. |
| `sdk/client_entity_list.h` / `game/entity_list.cpp` | Named `VClientEntityList003` interface and player-slot translation. |
| `sdk/entity/*.h` | Netvar-backed entity accessors and the two client-only accessors (crosshair target, x86 dormancy). |
| `sdk/client_offsets.h` | Every build-specific client-only field offset and code shape, grouped by the aidocs section that holds its evidence. |
| `config/signatures.ini` / `config/signatures-x64.ini` | Architecture-specific patterns, named interfaces, operand decoders, pointer indirections, validation settings, feature defaults, and discovery links. |
| `game/interfaces.cpp` | `CreateInterface` lookup plus configured entity-list, render/model interfaces, view-matrix retrieval, EngineClient, EngineTrace, ClientState, and ClientMode resolution logic. |
| `game/model.cpp` / `math/projection.cpp` | Checked studio parent-link lookup and `VMatrix`-convention world-to-screen projection used by the experimental Bone ESP path. |
| `hooks/hooks.cpp` / `hooks/d3d9_device.cpp` | Vtable slots for `OverrideView`, `CreateMove`, `VGUI_Surface030::LockCursor`, `EndScene`, `Reset`, and `IDirect3DDevice9Ex::ResetEx`. |
| `core/constants.h` | Entity stride, player limits, team values, and movement flags. |

The debug dump reports module bases, runtime interface/client-only offsets, resolved netvar offsets, the resolved ClientState address, view angles from `VEngineClient`, the local bone-cache pointer/count, Bone ESP view/matrix/hierarchy/projected-line status, aimbot valid/readable/visible target counts, and the local player position when one is available. If a signature or required netvar is not found, or a read produces null/garbage data, treat the binary and the offsets as mismatched and re-dump them rather than guessing.

## Tutorial notes

- [001 - Signature scanning, offsets, and pointer derivation](aidocs/001_signature-scanning-and-offsets.md)
- [002 - Netvars and entity offsets](aidocs/002_netvars-and-entity-offsets.md)
- [003 - Global addresses, interfaces, and input commands](aidocs/003_global-addresses-and-inputs.md)
- [004 - x64 migration and ABI](aidocs/004_x64-migration-and-abi.md)
- [005 - No-spread and weapon accuracy](aidocs/005_no-spread-and-weapon-accuracy.md)
- [006 - Bone ESP and world-to-screen projection](aidocs/006_bone-esp-and-world-to-screen.md)
- [007 - A weapon-wheel menu](aidocs/007_weapon-wheel-menu.md)

The architecture-selected signature profile is the source of truth for the
runtime signatures and named client/engine interfaces. It intentionally
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
| `EngineTrace interface not found` or all targets are invisible | `EngineTraceClient003`, `TraceRay` slot 4, the `Ray_t`/`CGameTrace` layout, or the entity-skip filter does not match the loaded engine. Re-check the x64 evidence in `aidocs/004`. |
| `Failed to initialize netvars` | `GetAllClasses`, a required RecvTable, or a required property does not match this client build. Stop and re-check the table path and pointer-width assertions. |
| D3D9 capture fails or the menu never appears | The code needs a visible, suitably sized game window and a D3D9 device. Wait until the game window is initialized and verify that the target is using D3D9. |
| The game cannot recover its device after a resolution change or alt-tab | Confirm that startup logged the `Reset` address and hooked it. If it warns that the D3D9Ex `Reset` differs, that entry is not hooked yet; see `aidocs/007`, section 7.1. |
| The menu opens but the cursor stays pinned to the center | Confirm `VGUI_Surface030` resolves and `LockCursor` is logged. Re-check surface slots 61/62 and the engine `CalculateMouseVisible`/`IsCursorLocked` path documented in `aidocs/004`; a Win32-only cursor change cannot stop Source input recentering. |
| A feature crashes or reads implausible values | Stop testing: an entity/global offset is stale or the target is not the expected architecture/build. Disable the feature and return to Ghidra. |

## Known limitations

- Signatures, interface versions, vtable assumptions, and remaining client-only fields are tied to the client build this project was developed against.
- Netvars remove several hardcoded entity displacements, but the `ClientClass`/`RecvTable` ABI, table names, property names, and type assumptions are still build-family dependencies.
- Memory access is direct and lightly validated; this is experimental code, not a hardened runtime.
- The aimbot remains intentionally basic: closest visible target, bone 14, and an immediate angle change. It does not implement smoothing, weapon handling, or movement correction; visibility is a client trace approximation and is not a server-side visibility guarantee.
- The triggerbot is deliberately narrow: it uses the client-only crosshair target ID and requires the local player to be on the ground.
- Rendering support is D3D9-specific and depends on finding the game's visible top-level window.
- The menu wheel is laid out in fixed pixels for 1080p, so it is smaller at higher resolutions and larger at lower ones. Its input, and the `Reset`/`ResetEx` handling, have not been tested in the game yet.
- The deterministic seed/spread math has an offline CTest target; signatures, interfaces, timing, and object lifetimes still require an architecture-matched build followed by a permitted local smoke test.
- Perfect no-spread is validated only for the documented x64 sample path and remains off by default; the x86 cone path and other weapon branches require their own runtime evidence.
- Bone ESP is an x64-first read-only experiment and remains off by default; the x86 interface slots, studio layout, cache timing, and projection path require their own Ghidra and runtime validation.
- Bone ESP currently draws valid enemy skeletons from cached matrices but does not yet claim authoritative visibility, interpolation correctness, or resilience across unrelated engine builds.
- Nothing here is intended to bypass VAC, FaceIT, or any other anti-cheat system.

## Videos / notes while reversing

| Topic | Link |
|-------|------|
| Finding ClientState | [Odysee](https://odysee.com/@Swiss-Experiments:a/finding-clientstate-with-ida-on-counter:7) · [YouTube](https://www.youtube.com/watch?v=J6vO-ANi4Q8) |
| Finding view angles | [Odysee](https://odysee.com/@Swiss-Experiments:a/finding-viewangles-with-ida-for-counter:e) · [YouTube](https://www.youtube.com/watch?v=mS8ZQ5N7Dvk) |
| Finding bone matrix | [Odysee](https://odysee.com/@Swiss-Experiments:a/how-to-locate-bonematrix:5) · [YouTube](https://www.youtube.com/watch?v=elKUMiqitxY) |

The client-only offsets in `sdk/client_offsets.h` are for the client builds this project was verified against. The x64 bone-cache offsets documented in `aidocs/004` came from the current `SetupBones` implementation, but they still need to be re-dumped if the binary changes.

The signature records point back to this section and the numbered tutorial so the pattern bytes, operand offsets, and pointer-chain assumptions can be re-derived instead of copied blindly.

## Status / honesty

- Code quality is tutorial and experimental, not production.
- There is no support for protected multiplayer or anti-cheat bypass.
- If you need a maintained end-user package, this repository is intentionally not that.

## End goal

Understand the client well enough to defend a large CS:S community: know what cheats can see and do, and how to reason about them when they show up on the server.
